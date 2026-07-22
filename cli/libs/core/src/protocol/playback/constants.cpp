// getvinfo_constants.cpp — Xem getvinfo_constants.h. Port 1:1 từ
// ../analytics/app/getvinfo_constants.py. Giá trị transcribe nguyên văn; test byte-exact
// (test_capability) so với Python để bắt sai sót gõ.
#include "tencent/core/protocol/playback/constants.h"

#include <stdexcept>

namespace tencent::playback {
namespace constants {

// getvinfo_constants.py:67-108
OrderedKV capability_bits() {
  return {
      {"spaudio", "3471"},
      {"spaudioeffectver", "5"},
      {"spau", "284"},
      {"dolbydecode", "40"},
      {"spvideo", "5124"},
      {"spvfps", "240"},
      {"sphevcfps", "70368744177663"},
      {"spvvcfps", "47"},
      {"spavs3", "15"},
      {"spsfrhdr", "300"},
      {"spuhdsfrhdr", "200"},
      {"sphls", "14"},
      {"spm3u8tag", "19027"},
      {"spmasterm3u8", "19"},
      {"spcaptiontrack", "1"},
      {"spcaptiontype", "1"},
      {"spsrt", "3"},
      {"sprichtype", "all"},
      {"sprichver", "2.1.0"},
      {"spadseg", "3"},
      {"spwm", "8"},
      {"spvvpay", "0"},
      {"sppreviewtype", "1"},
      {"ssv", "13"},
      {"sptxvdrm", "63"},
      {"hevclv", "33"},
      {"spptype", "4,5,6,7,8,9,10,11,12,101"},
  };
}

// getvinfo_constants.py:114-120
OrderedKV format_hints() {
  return {
      {"encryptVer", "4.2"},
      {"defnpayver", "1"},
      {"otype", "ojson"},
      {"dtype", "3"},
      {"spgzip", "1"},
  };
}

// getvinfo_constants.py:129-145
OrderedKV player_ctrl_defaults() {
  return {
      {"playctrl", "274"},
      {"appctrl", "507905"},
      {"subctrl", "2"},
      {"livequeue", "1"},
      {"adversion", "220220"},
      {"adchid", "0"},
      {"plat_bucketid", "0"},
      {"drm", "1032"},
      {"defn", "uhd"},
      {"defnsrc", "3"},
      {"defnswitch", "0"},
      {"clip", "4"},
      {"retry", "0"},
      {"scene", "playerUiType=0"},
      {"dsextinfo", "sourcetype=0&abtest=-1"},
  };
}

// getvinfo_constants.py:152-160
OrderedKV device_fixed() {
  return {
      {"platform", "10303"},
      {"sdtfrom", "v5000"},
      {"appVer", kAppVersion},
      {"manufacturer", "samsung"},
      {"devicemodel", ""},
      {"osver", "14"},
      {"screeninfo", "1080*2176"},
  };
}

// getvinfo_constants.py:224-227
OrderedKV empty_defaults() {
  return {
      {"qq", ""},
      {"srccontenid", ""},
  };
}

// getvinfo_constants.py:245-251 — gộp đúng thứ tự (không có key trùng giữa 5 bảng).
OrderedKV safe_static_constants() {
  OrderedKV out;
  auto extend = [&out](const OrderedKV& part) {
    out.insert(out.end(), part.begin(), part.end());
  };
  extend(capability_bits());
  extend(format_hints());
  extend(player_ctrl_defaults());
  extend(device_fixed());
  extend(empty_defaults());
  return out;
}

OrderedKV request_param_defaults() {
  return {
      {"defnpayver", "1"},
      {"spvvcfps", "47"},
      {"bitrate", "0"},
      {"sppreviewtype", "1"},
      {"platform", "10303"},
      {"retry", "0"},
      {"newnettype", "1"},
      {"flowid", ""},
      {"encryptVer", "4.2"},
      {"vid", ""},
      {"spvvpay", "0"},
      {"spuhdsfrhdr", "200"},
      {"subctrl", "2"},
      {"spaudio", "3471"},
      {"defnswitch", "0"},
      {"ipstack", "3"},
      {"defn", "uhd"},
      {"spgzip", "1"},
      {"qq", ""},
      {"plat_bucketid", "0"},
      {"spaudioeffectver", "5"},
      {"manufacturer", ""},
      {"spmasterm3u8", "19"},
      {"clip", "4"},
      {"rec_user_level", "user_level_I"},
      {"reportparams", "orispvvcfps=47&abtestid=&fwexpid=&spvvcfpsreasonnew=0&fwtype=2"},
      {"dolbydecode", "40"},
      {"adversion", "220220"},
      {"livequeue", "1"},
      {"spptype", "4,5,6,7,8,9,10,11,12,101"},
      {"sprichtype", "all"},
      {"osver", ""},
      {"cid", ""},
      {"sphevcfps", "70368744177663"},
      {"srccontenid", ""},
      {"otype", "ojson"},
      {"adpass", ""},
      {"detail_user_level", "79"},
      {"dsextinfo", "sourcetype=0&abtest=-1"},
      {"openid", ""},
      {"defnsrc", "3"},
      {"costdivide", "videoType%3D2"},
      {"user_level", "79"},
      {"adchid", "0"},
      {"appctrl", "507905"},
      {"playctrl", "274"},
      {"hevclv", "33"},
      {"spwm", "8"},
      {"dtype", "3"},
      {"spm3u8tag", "19027"},
      {"userid", ""},
      {"spsrt", "3"},
      {"spcaptiontrack", "1"},
      {"sphls", "14"},
      {"spavs3", "15"},
      {"spcaptiontype", "1"},
      {"drm", "1032"},
      {"scene", "playerUiType=0"},
      {"ssv", "13"},
      {"viptype", "3"},
      {"sptxvdrm", "63"},
      {"spvideo", "5124"},
      {"spsfrhdr", "300"},
      {"mediascene", "fwexpid=&fwtype=2&ctime=0&pluginadctrl=1"},
      {"appVer", kAppVersion},
      {"atime", "0"},
      {"sprichver", "2.1.0"},
      {"spadseg", "3"},
      {"spvfps", "240"},
      {"spau", "284"},
      {"guid", ""},
      {"qimei", ""},
      {"abuserid", ""},
      {"sdtfrom", "v5000"},
      {"devicemodel", ""},
      {"screeninfo", ""},
      {"cKey", ""},
  };
}

// getvinfo_constants.py:170-190. None -> std::nullopt (lấy từ template).
OptionalKV per_request_runtime() {
  return {
      {"bitrate", std::nullopt},
      {"newnettype", std::nullopt},
      {"ipstack", std::nullopt},
      {"atime", std::nullopt},
      {"mediascene", std::string("fwexpid=&fwtype=2&ctime=0&pluginadctrl=1")},
      {"reportparams",
       std::string("orispvvcfps=47&abtestid=&fwexpid=&spvvcfpsreasonnew=0&fwtype=2")},
  };
}

// getvinfo_constants.py:211-213
OrderedKV normal_path_additions() {
  return {
      {"costdivide", "videoType%3D2"},  // URLEncoder.encode("videoType=2")
  };
}

// getvinfo_constants.py:215-218
OrderedKV history_path_required() {
  return {
      {"previd_type", "0"},
      // "previd": caller phải tự build/copy JSON
  };
}

// getvinfo_constants.py:260-267
OrderedKV path_additions(const std::string& mode) {
  if (mode == "normal") return normal_path_additions();
  if (mode == "history") return history_path_required();
  throw std::invalid_argument("unknown mode: " + mode + "; expected 'normal' or 'history'");
}

// getvinfo_constants.py:235-239
std::vector<std::string> per_request_generated_keys() {
  return {"flowid", "cKey", "ckey_cost"};
}

}  // namespace constants
}  // namespace tencent::playback
