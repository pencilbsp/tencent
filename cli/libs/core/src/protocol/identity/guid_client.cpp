#include "tencent/core/protocol/identity/guid_client.h"

#include <chrono>
#include <cstring>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

#include "tencent/core/net/http_client.h"
#include "tencent/core/common/ordered_kv.h"

namespace tencent::guid {
namespace {

constexpr int kCmd = 57728;

void be16(std::string& out, uint16_t v) {
  out.push_back(static_cast<char>(v >> 8)); out.push_back(static_cast<char>(v));
}
void be32(std::string& out, uint32_t v) {
  for (int i=3;i>=0;--i) out.push_back(static_cast<char>(v>>(i*8)));
}
void be64(std::string& out, uint64_t v) {
  for (int i=7;i>=0;--i) out.push_back(static_cast<char>(v>>(i*8)));
}
uint16_t get16(const std::string& s, std::size_t p) {
  if (p+2>s.size()) throw std::runtime_error("GUID truncated u16");
  return (uint16_t(uint8_t(s[p]))<<8)|uint8_t(s[p+1]);
}
uint32_t get32(const std::string& s, std::size_t p) {
  if (p+4>s.size()) throw std::runtime_error("GUID truncated u32");
  uint32_t v=0; for(int i=0;i<4;++i)v=(v<<8)|uint8_t(s[p+i]); return v;
}
void head(std::string& out, int type, int tag) {
  if (tag<15) out.push_back(static_cast<char>((tag<<4)|type));
  else { out.push_back(static_cast<char>(0xf0|type)); out.push_back(static_cast<char>(tag)); }
}
void integer(std::string& out, int tag, int64_t value) {
  if (value==0) { head(out,12,tag); return; }
  if (value>=-128 && value<=127) {
    head(out,0,tag); out.push_back(static_cast<char>(value)); return;
  }
  if (value>=-32768 && value<=32767) {
    head(out,1,tag); be16(out,static_cast<uint16_t>(value)); return;
  }
  if (value>=INT32_MIN && value<=INT32_MAX) {
    head(out,2,tag); be32(out,static_cast<uint32_t>(value)); return;
  }
  head(out,3,tag); be64(out,static_cast<uint64_t>(value));
}
void string(std::string& out, int tag, const std::string& value) {
  if (value.size()<=255) {
    head(out,6,tag); out.push_back(static_cast<char>(value.size()));
  } else {
    head(out,7,tag); be32(out,static_cast<uint32_t>(value.size()));
  }
  out+=value;
}
void bytes_field(std::string& out, int tag, const std::string& value) {
  head(out,13,tag); head(out,0,0); integer(out,0,value.size()); out+=value;
}
void structure(std::string& out, int tag, const std::string& body) {
  head(out,10,tag); out+=body; head(out,11,0);
}
void float_field(std::string& out, int tag, float value) {
  head(out,4,tag); uint32_t bits=0; std::memcpy(&bits,&value,4); be32(out,bits);
}
void double_field(std::string& out, int tag, double value) {
  head(out,5,tag); uint64_t bits=0; std::memcpy(&bits,&value,8); be64(out,bits);
}
void map_field(std::string& out, int tag,
               const std::vector<std::pair<std::string,std::string>>& values) {
  head(out,8,tag); integer(out,0,values.size());
  for (const auto& [k,v] : values) { string(out,0,k); string(out,1,v); }
}

std::string build_qua(const Context& c) {
  std::string extent; integer(extent,0,0); integer(extent,1,0); string(extent,2,"");
  std::string coords; integer(coords,0,0); float_field(coords,1,0); float_field(coords,2,0);
  double_field(coords,3,0);
  std::vector<std::pair<std::string,std::string>> ext={
      {"QIMEI36",c.qimei36},{"usid",c.usid},{"device_model_type",c.model},
      {"skp_mode","0"},{"manufacturer",c.manufacturer},{"brandAndModel",c.model},
      {"preview_mode","0"},{"new_mark_label_enabled","0"},
      {"new_slim_recommend_ui_enabled","0"},{"is_read_screen","0"},{"is_fold","0"}};
  if (!c.os_version.empty()) ext.push_back({"osVersion","Android_"+c.os_version});
  std::string out;
  string(out,0,c.version_name); string(out,1,c.version_code);
  integer(out,2,c.screen_width); integer(out,3,c.screen_height); integer(out,4,3);
  string(out,5,"Android_"+c.os_version); integer(out,6,1); integer(out,7,4);
  integer(out,8,c.screen_dpi); string(out,9,c.channel_id);
  string(out,10,""); string(out,11,""); string(out,12,""); string(out,13,c.omg_id);
  string(out,14,""); structure(out,15,extent); string(out,16,""); string(out,17,"");
  string(out,18,""); structure(out,19,coords); string(out,20,""); string(out,21,c.model);
  integer(out,22,1); integer(out,23,0); integer(out,24,0); integer(out,25,0);
  integer(out,26,0); string(out,27,""); string(out,28,""); map_field(out,29,ext);
  return out;
}

std::string build_head(const Context& c, uint64_t request_id) {
  std::string log;
  string(log,0,""); string(log,1,""); integer(log,2,0); string(log,3,"");
  integer(log,4,0); string(log,5,""); string(log,6,""); string(log,7,"");
  string(log,8,c.channel_id); string(log,9,""); string(log,10,"");
  std::string safe; integer(safe,0,3); string(safe,1,"");
  std::string business; integer(business,0,0);
  std::string out;
  integer(out,0,request_id); integer(out,1,kCmd); structure(out,2,build_qua(c));
  string(out,3,"1000005"); string(out,4,""); structure(out,6,log);
  integer(out,8,0); integer(out,9,0); integer(out,10,0);
  structure(out,11,safe); structure(out,12,business);
  return out;
}

class Reader {
 public:
  explicit Reader(const std::string& data) : data_(data) {}
  std::pair<int,int> next_head() {
    if (off_>=data_.size()) throw std::runtime_error("GUID JCE head truncated");
    uint8_t b=data_[off_++]; int type=b&15,tag=b>>4;
    if(tag==15) { if(off_>=data_.size()) throw std::runtime_error("GUID JCE tag truncated");
      tag=uint8_t(data_[off_++]); }
    return {type,tag};
  }
  int64_t integer_value(int type) {
    if(type==12)return 0;
    int size=type==0?1:type==1?2:type==2?4:type==3?8:0;
    if(!size||off_+size>data_.size()) throw std::runtime_error("GUID JCE integer invalid");
    uint64_t v=0; for(int i=0;i<size;++i)v=(v<<8)|uint8_t(data_[off_++]);
    if(size<8 && (v&(uint64_t(1)<<(size*8-1)))) v|=(~uint64_t(0))<<(size*8);
    return static_cast<int64_t>(v);
  }
  std::string value_bytes(int type) {
    if(type==6) {
      if(off_>=data_.size()) throw std::runtime_error("GUID JCE string truncated");
      std::size_t n=uint8_t(data_[off_++]); return take(n);
    }
    if(type==7) { std::size_t n=read_u32(); return take(n); }
    if(type==13) {
      auto elem=next_head(); if(elem.first!=0) throw std::runtime_error("GUID JCE byte list invalid");
      auto len=next_head(); return take(static_cast<std::size_t>(integer_value(len.first)));
    }
    throw std::runtime_error("GUID JCE value is not bytes");
  }
  void skip(int type) {
    if(type<=3||type==12) { integer_value(type); return; }
    if(type==6||type==7||type==13) { value_bytes(type); return; }
    if(type==4) { take(4); return; } if(type==5) { take(8); return; }
    if(type==8) {
      auto count=next_head(); int64_t n=integer_value(count.first);
      for(int64_t i=0;i<n;++i) {
        auto key=next_head(); skip(key.first);
        auto value=next_head(); skip(value.first);
      }
      return;
    }
    if(type==9) {
      auto count=next_head(); int64_t n=integer_value(count.first);
      for(int64_t i=0;i<n;++i) { auto item=next_head(); skip(item.first); }
      return;
    }
    if(type==10) { while(true){auto h=next_head();if(h.first==11)break;skip(h.first);} return; }
    if(type==11) return;
    throw std::runtime_error("GUID JCE skip unsupported");
  }
  bool done() const { return off_>=data_.size(); }
 private:
  std::string take(std::size_t n) {
    if(off_+n>data_.size()) throw std::runtime_error("GUID JCE bytes truncated");
    std::string out=data_.substr(off_,n); off_+=n; return out;
  }
  uint32_t read_u32() {
    if(off_+4>data_.size()) throw std::runtime_error("GUID JCE u32 truncated");
    uint32_t v=get32(data_,off_); off_+=4; return v;
  }
  const std::string& data_; std::size_t off_=0;
};

uint64_t random_request_id() {
  std::random_device rd; std::mt19937_64 gen(rd());
  return std::uniform_int_distribution<uint64_t>(1,0x7fffffff)(gen);
}

}  // namespace

std::string build_recovery_frame(const Context& c, uint64_t request_id, int64_t now_sec) {
  std::string body; integer(body,0,1); integer(body,1,2); integer(body,2,now_sec);
  std::string command; structure(command,0,build_head(c,request_id)); bytes_field(command,1,body);
  std::string jce; jce.push_back(0x26); be32(jce,command.size()+17);
  jce.push_back(1); jce.append(10,'\0'); jce+=command; jce.push_back(0x28);
  std::string payload=http::gzip_compress(jce);
  std::string out; out.push_back(0x13); be32(out,0); be16(out,1); be16(out,65281);
  be16(out,kCmd); be16(out,0); be64(out,request_id); be32(out,531); be32(out,10012);
  be64(out,0); out.append(32,'\0'); out.push_back(1);
  int version=0; try { version=std::stoi(c.version_code); } catch (...) {}
  be32(out,version); out.append(6,'\0'); out.push_back(0); be16(out,0); be16(out,0);
  be32(out,jce.size()); out+=payload; out.push_back(0x03);
  uint32_t length=out.size();
  out[1]=static_cast<char>(length>>24);out[2]=static_cast<char>(length>>16);
  out[3]=static_cast<char>(length>>8);out[4]=static_cast<char>(length);
  return out;
}

Result parse_response(const std::string& raw_input) {
  std::string data=http::maybe_gunzip(raw_input);
  if(data.size()<88||uint8_t(data[0])!=0x13) throw std::runtime_error("GUID response is not QMF");
  uint16_t cmd=get16(data,9), access=get16(data,11); uint32_t flags=get32(data,21);
  if(cmd!=kCmd||access!=0) throw std::runtime_error("GUID response command/access error");
  std::size_t off=25+4+8+32+1+10+1;
  uint16_t first=get16(data,off);off+=2+first;
  uint16_t second=get16(data,off);off+=2+second;
  uint32_t unpacked=get32(data,off);off+=4;
  if(data.back()!=0x03||off>=data.size()) throw std::runtime_error("GUID QMF payload invalid");
  std::string payload=data.substr(off,data.size()-off-1);
  if((flags&2)&&(flags&16)) payload=http::gunzip(payload);
  if(unpacked&&payload.size()!=unpacked) throw std::runtime_error("GUID payload length mismatch");
  if(payload.size()<17||uint8_t(payload[0])!=0x26||uint8_t(payload.back())!=0x28)
    throw std::runtime_error("GUID inner JCE frame invalid");
  std::string command=payload.substr(16,payload.size()-17), response_body;
  Reader command_reader(command);
  while(!command_reader.done()) {
    auto h=command_reader.next_head(); if(h.first==11)break;
    if(h.second==1) response_body=command_reader.value_bytes(h.first);
    else command_reader.skip(h.first);
  }
  Result result; Reader body_reader(response_body);
  while(!body_reader.done()) {
    auto h=body_reader.next_head(); if(h.first==11)break;
    if(h.second==0) result.err_code=static_cast<int>(body_reader.integer_value(h.first));
    else if(h.second==1) result.err_message=body_reader.value_bytes(h.first);
    else if(h.second==2) result.guid=body_reader.value_bytes(h.first);
    else if(h.second==3) result.server_time=body_reader.integer_value(h.first);
    else body_reader.skip(h.first);
  }
  return result;
}

Result recover(const Context& context, long timeout_sec) {
  if(context.qimei36.empty()) throw std::runtime_error("GUID recovery requires qimei36");
  int64_t now=std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  std::string frame=build_recovery_frame(context,random_request_id(),now);
  OrderedKV headers={{"uqm",context.qimei36},
                     {"vb-request-tag","vb_net_retry_backoff_config=B"},
                     {"JceGodId",std::to_string(kCmd)}};
  auto response=http::http_post("https://acc.qq.com/",headers,frame,timeout_sec);
  if(response.first!=200) throw std::runtime_error("GUID HTTP "+std::to_string(response.first));
  Result result=parse_response(response.second);
  if(result.err_code!=0||result.guid.empty())
    throw std::runtime_error("GUID server error "+std::to_string(result.err_code)+
                             (result.err_message.empty()?"":": "+result.err_message));
  return result;
}

}  // namespace tencent::guid
