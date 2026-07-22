// AUTO-GENERATED bởi tests/gen_vectors.py — KHÔNG SỬA TAY.
// Nguồn ground-truth: ../analytics/app/parse_ckey_v42.py, ../analytics/app/encode_ckey_v42.py
// + capture thật /Users/pencil/.tencent/session_template.json.
// Sinh lại: python3 tests/gen_vectors.py   (AGENTS.md §1)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vec {

// poly131(bytes(range(64)))
inline const char* POLY131_IN_HEX = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f";
inline const uint32_t POLY131_OUT = 0x5e116f20u;

// tencent_b64_encode — kiểm tra cả hai nhánh padding (rem==2 và rem==1)
inline const char* B64_IN_HEX  = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f3031";
inline const char* B64_OUT     = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDE";
inline const char* B64_IN2_HEX = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f30";
inline const char* B64_OUT2    = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMA";

// xor_bytes(bytes(range(40)), key)
inline const char* XOR_IN_HEX  = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f2021222324252627";
inline const char* XOR_KEY_HEX = "842eed08f066e6ea48b4caa991ed6ff3";
inline const char* XOR_OUT_HEX = "842fef0bf463e0ed40bdc0a29de061fc943fff1be473f0fd50add0b28df071eca40fcf2bd443c0cd";

// TEA block đơn (tea_key_sample, block=bytes(range(8)))
inline const char* TEA_KEY_HEX       = "7947a1d19371b533d9f6bbc83de4c253";
inline const char* TEA_BLOCK_IN_HEX  = "0001020304050607";
inline const char* TEA_BLOCK_ENC_HEX = "e04277d8882fe28d";
inline const char* TEA_BLOCK_DEC_HEX = "dd4d6d5347df081f";

// QQ-TEA CBC dual-state (input=bytes(range(32)))
inline const char* CBC_IN_HEX  = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
inline const char* CBC_ENC_HEX = "e04277d8882fe28dc819cf1d59cec04640c4eac2aa8eeb72584cb6be882b76ed";
inline const char* CBC_DEC_HEX = "dd4d6d5347df081fa22bc7a7cf55757eae700c9f384356b59c23c4198c873c89";

// cKey v4.2 đầy đủ — từ template CAPTURE thật, entropy tất định.
inline const char* CKEY_PREAMBLE_HEX = "0000421500000015000011d70000283f000000006a14811a";
inline const std::vector<std::string> CKEY_FIELDS_HEX = {
      "66636567",
      "",
      "392e30322e39302e3331313039",
      "393837666634376130316361",
      "3263336463396238323661373131663139313639376338633039323638666361",
      "636f6d2e74656e63656e742e71716c697665",
      "636f6d2e74656e63656e742e71716c697665",
      "3444453143373342",
      "6e756c6c",
      "636f6d2e74656e63656e742e71716c697665",
      "3444453143373342",
      "6e756c6c",
      "",
      "382e312e322e3136302e34",
      "7b226561223a332c2273647270223a312c2273657373696f6e4944223a302c22737061223a312c227370656e636c76223a322c2273736b223a226b68796d353533736830227d",
      "7b22626174223a2278376b706d39776c3376222c2263666c73223a5b2237347a786a3678643739225d2c226370223a2237347a786a3678643739222c226373616c223a5b226d3568307a6368726835222c223978376b367563377877222c2270706c726f30716a3072222c226b3774736c696d713838222c226f34737465693675746e225d2c226373666c64223a2237347a786a3678643739222c2263737465223a2238716e63687a7263686c222c226464223a2231386331386535313862633366633262313737383230363235363237353034653130222c226c76223a2276342e30222c227364223a223137373937323836363030393832393265366430313137396238383732227d",
      "373031316563663532333565343039663036616531393134313030303138393161333137",
      "373031316563663532333565343039663036616531393134313030303138393161333137",
      "4c4b85018eaa000a144080ac633eaf64d60c73f0e26648c66b1b",
      "3137373937323836363030393832393265366430313137396238383732",
      "30343364613436633066633035643933",
      "392e30322e39302e3331313039",
      "",
      "",
      "534d2d473939314e",
      "362e3232",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "00",
      "",
      "",
      ""
};
inline const uint32_t CKEY_RAW_BITMAP = 0x00000007u;
inline const std::vector<uint32_t> CKEY_RAW_INTS = {0, 0, 0};
inline const char* CKEY_TRAILING_HEX = "0b382e312e322e3136302e340000000000000006";
inline const char* CKEY_TEA_KEY_HEX  = "7947a1d19371b533d9f6bbc83de4c253";
inline const char* CKEY_XOR_KEY_HEX  = "842eed08f066e6ea48b4caa991ed6ff3";
inline const char* CKEY_RAND2_HEX    = "a0a1";   // os.urandom(2)
inline const uint8_t CKEY_HEAD_RAND1 = 0xa2u;  // os.urandom(1)[0]
inline const char* CKEY_EXPECTED     = "--01Tt_ls3EgULO5XwIueGIhCg4YjlY-ZE9kBNzyiBW5yIbmOe8j_QEaQqn3KLe258-a2wm1TWt-JI4tlTS90Z5Tj-_gHWr0qrlgLni6Z2Ms61U9RVlJU6MvXJydAuHwnz3xFezTt3Y30mw-AcNgBmcISA3gDO9pu6vY2dsPKcDHv51WGFD2cKM6GRQmck8H-9z4ZdZpwTJafpu8LCGiw1a3oRRQsFnO3cBmJ2MrxQXTX_XwzO54DJG-bNgMpTKvtnAXXL6sQgdQgbk-7rj-fceAboDPELw-_6nIupHbFIQllUa8XjkqXomht_X6hfd2uQ6zsfTSReZWOuhK320BtgkwkbKetNyNJrXtzuUogOFNAk8q3G-_i8nonOTkyPh7vGB46Q2NPiOp14uRRpcwm9hu4w9oi0kazPV8oP5m8ye1EBrgew3Lhh49BaDQf-EYWY0IRaV5zKcY80-ZB13V5p534-SO2SfLgxw8f4FH31PrXEEMZaCwHtGK42Om3woagIi8QLADymc_5W4pUYhEXLZwek8Cq9vAq3Wrz6nPAO-D3TuLAU69X6-rgPXlNvbvixr5CUSn4P6u1bnf6XmbJQ-RzZS6BSaivKqKpWlnU0-6-ikOCi5LL21_elhYeAQ0_-LchmSzvgV3wFKiGxCraNUrt9t_HCihaVVjTMNKx2usR1XWgshHMen_DObL0zldu0iwdyERiDSOj9HU4sTtw-IFbXPxw4klaqmf06hYeRvoHJKA4gvH6HctjvSi74H7o2iAdruJ40KXAzszZF7PWj4n5OcayZPsURoZCrFSHmgqk5oghDUPH4TUEj0E2zl4E4tpy47Pf0QXNQnFCT2aXKnuzyFEC7CICM-jqEBHyyu_r-SiwsWT2rO6StsxoetyhGKjdjHN86ckf_EP1jHakM970hGAwJOQ7KuvHe9eFcajUYt0MqjJXRvsZhwfgDUYz4edvq_1t7A87PhFA6cFgiCGKCKbWmideSWoO3Vupn5dWgGYesSX3Hk1RKQ2Ml6VQrvzL4hyCK_fGUqf1vR6Hz0eFHPbmmw1FZEDRXiJ7EN1HkubBjHS-sCZPypUePHql5ZfirDxyA";

// ── DECODE vector — cKey CAPTURE thật (request.authdump.txt) ──────────────────
// decode_ckey(CAP_CKEY) phải cho ra ĐÚNG các trường dưới đây (so với Python
// parse_full_captured). cKey capture có pad ngẫu nhiên nên KHÔNG roundtrip được
// — roundtrip được kiểm trên CKEY_EXPECTED (do encoder của ta sinh, pad=0).
inline const char* CAP_CKEY            = "--01hBM700qXaSxgd6T216pA5lRKyhXIZyfAgp4AIFHWxWE2nvQNjqUp1pxesmfai6a0JhA39SfZw14GgvRHFuzkDZHom-UKQXOrooRWjKYrCE90L_96pkiUZw0qFiP0qnQ0d_ANtL7LQ_1GZTNRFTLGDKyB6vpYQAw_jg4iST_a8UtjmSZo1-dzBUGNroIjc3y6AZHzMQ1YXeRmD61iHVfDdeA644GiAbYNpyMZ8zCeyB7EzIYswuZcs0aOliWzfNvBA3yMhrHVAWmq3JAp7fknDDMD0BEq08jKXZAavsQzGXBjbJ7FNLltxIt2HgU5nuz91L6FLod3qe4K-UQZkKPM9waxPjd-YAImQlVp089bAvfHGvNXDjsveSaXcDFGsiLlzuDMYvbFHCVIA6QXZCaN4FVZyX376eWYMOOx5CtqIM4DPyBW7j5rCCNrmCALijK6Q9AOWCx6oe7OUCu0cGgmdxAFstlxc19E6oDca0BuxId5vI_4faEp04aBcUl8IhMn78wsmNGCcomPB5QBQKIc7U61Z7WTy3wFEdwEmhE6rw0dKSMESkXtr1VwgpY92D7uDjiexFMQiQnTnAmuOsHs_APyfC_jWLcr3ufeLwbFQrqrMBT2dP9dBHFcqa4w2C3GGquYztyng-AoD6WEGo5dUkmRggjiJHkv-gp4SQzQhdMANJQ8qwtmMynuQ1YBse6RJ5gRY_Wa2wD603o0LaOu8gR0BirIc-LrvJ8-iiB_FlRoFVzCP3spY2vYtcxeh0tSRRl5LCwEtt5Rp9tXp8fXmifWffQSapvzleuUPjbOR62hTp9_uwOLTF_BKQme3vXImCF1VDSi-9ZdXWvwKBHEYRI_X9KtH73KblY0Pab3mZDpoo8BZfivH1UydzPX9Cbynqh_JGT0kN6KnCiWtNzHD2BZjlbeM2Z78V9HUZkp4muNynNl13Qqk1iX0sH1HqB-H85L0POL2blbNTHR3YcsUW3jDPIN8SaIy2sYC1bM0Oo_yiiizqiFEBvKKSdif8hV49vw9EGum7AkyrEyh6PwCHpr2i7xlzx6DUsbLw";
inline const char* CAP_PREAMBLE_HEX    = "0000421500000015000011d70000283f000000006a1826fd";
inline const std::vector<std::string> CAP_FIELDS_HEX = {
      "66636567",
      "",
      "392e30322e39302e3331313039",
      "7634313032396f32377778",
      "3263336463396238323661373131663139313639376338633039323638666361",
      "636f6d2e74656e63656e742e71716c697665",
      "636f6d2e74656e63656e742e71716c697665",
      "3444453143373342",
      "6e756c6c",
      "636f6d2e74656e63656e742e71716c697665",
      "3444453143373342",
      "6e756c6c",
      "",
      "382e312e322e3136302e34",
      "7b226561223a332c2273647270223a312c2273657373696f6e4944223a302c22737061223a312c227370656e636c76223a322c2273736b223a226b68796d353533736830227d",
      "7b22626174223a2278376b706d39776c3376222c2263666c73223a5b2237347a786a3678643739225d2c226370223a2237347a786a3678643739222c226373616c223a5b226d3568307a6368726835222c223978376b367563377877222c2270706c726f30716a3072222c226b3774736c696d713838222c226f34737465693675746e225d2c226373666c64223a2237347a786a3678643739222c2263737465223a2238716e63687a7263686c222c226464223a2231386331386535313862633366633262313737383230363235363237353034653130222c226c76223a2276342e30222c227364223a223137373939363736323035343465663061666663326638646338356665227d",
      "373031316563663532333565343039663036616531393134313030303138393161333137",
      "373031316563663532333565343039663036616531393134313030303138393161333137",
      "4c4b850123c4000680f7ef2c384aaa1b88ed5af246e430b58a5c",
      "3137373939363736323035343465663061666663326638646338356665",
      "32613934343935326333353232343233",
      "",
      "",
      "",
      "534d2d473939314e",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "00",
      "",
      "",
      ""
};
inline const uint32_t CAP_RAW_BITMAP   = 0x00000007u;
inline const std::vector<uint32_t> CAP_RAW_INTS = {0, 0, 0};
inline const char* CAP_TRAILING_HEX    = "0b382e312e322e3136302e340000000000000004";
inline const int CAP_PAD_COUNT         = 1;
inline const uint16_t CAP_TLV_LEN      = 779u;
inline const uint32_t CAP_STORED_CS    = 0x45ffd186u;
inline const uint32_t CAP_COMPUTED_CS  = 0x45ffd186u;

// ── session_template loader vector — fixtures/session_template.sample.json ────
// SessionTemplate::parse(...) phải khôi phục ĐÚNG các giá trị dưới (so với Python
// SessionTemplate.load). query_params/headers phải GIỮ THỨ TỰ.
inline const int TPL_SCHEMA_VERSION = 1;
inline const double TPL_CAPTURED_AT = 1716663840.5;
inline const char* TPL_APP_VER      = "9.02.90.31109";
inline const char* TPL_SOURCE_PATH  = "tests/fixtures/sample_request.txt";
inline const char* TPL_XOR_KEY_HEX  = "842eed08f066e6ea48b4caa991ed6ff3";
inline const char* TPL_TEA_KEY_HEX  = "7947a1d19371b533d9f6bbc83de4c253";
inline const char* TPL_PREAMBLE_HEX = "000102030405060708090a0b0c0d0e0f1011121314151617";
inline const char* TPL_TRAILING_HEX = "0b00";
inline const uint32_t TPL_RAW_BITMAP = 0x00000007u;
inline const std::vector<uint32_t> TPL_RAW_INTS = {0, 0, 0};
inline const char* TPL_GUID        = "0000000000000000abcdef0123456789";
inline const char* TPL_QIMEI36     = "deadbeefcafebabe0011223344556677aabb";
inline const char* TPL_REQUEST_URL = "https://vv6.video.qq.com/getvinfo";
inline const char* TPL_COOKIES     = "video_platform=2;main_login=wx;ctime=123;";
inline const std::vector<std::string> TPL_QUERY_KEYS = {
      "vid",
      "defn",
      "platform",
      "sdtfrom",
      "encryptVer",
      "spvideo",
      "screeninfo",
      "drm",
      "hevclv",
      "spvvcfps",
      "spuhdsfrhdr"
};
inline const std::vector<std::string> TPL_QUERY_VALS = {
      "j4102syn9rp",
      "uhd",
      "10303",
      "v5000",
      "4.2",
      "5124",
      "1080*2176",
      "1032",
      "51",
      "31",
      "200"
};
inline const std::vector<std::string> TPL_HEADER_KEYS = {
      "Host",
      "User-Agent",
      "Accept-Encoding"
};
inline const std::vector<std::string> TPL_HEADER_VALS = {
      "vv6.video.qq.com",
      "qqlive",
      "gzip"
};

// ── capability + constants vector (Pha 3) ────────────────────────────────────
// serialize_params(...) của C++ phải khớp các chuỗi key=value (mỗi dòng một cặp)
// dưới đây (sinh từ Python capability_flags / getvinfo_constants).
// <NULL> = key bị xóa khỏi body.
inline const char* CAPS_DEFAULT      = "drm=1032\nsptxvdrm=63\nspvideo=5124\ndolbydecode=40\nsphls=14\nspm3u8tag=19027\nhevclv=33\nspavs3=15\nsphevcfps=70368744177663\nspvvcfps=47\nspsfrhdr=300\nspaudio=3471\nspaudioeffectver=5\nspau=284\nspuhdsfrhdr=200\n";
inline const char* CAPS_FROM_TPL     = "drm=1032\nsptxvdrm=63\nspvideo=5124\ndolbydecode=40\nsphls=14\nspm3u8tag=19027\nhevclv=51\nspavs3=15\nsphevcfps=70368744177663\nspvvcfps=31\nspsfrhdr=300\nspaudio=3471\nspaudioeffectver=5\nspau=284\nspuhdsfrhdr=200\n";
inline const char* CAPS_VIDEO        = "drm=1032\nsptxvdrm=63\nspvideo=1028\ndolbydecode=20\nsphls=14\nspm3u8tag=19027\nhevclv=33\nspavs3=15\nsphevcfps=70368744177663\nspvvcfps=47\nspsfrhdr=300\nspaudio=3471\nspaudioeffectver=5\nspau=284\nspuhdsfrhdr=200\n";
inline const char* CAPS_BASIC        = "drm=8\nsptxvdrm=0\nspvideo=0\ndolbydecode=0\nsphls=14\nspm3u8tag=19027\nhevclv=33\nspavs3=0\nsphevcfps=70368744177663\nspvvcfps=0\nspsfrhdr=0\nspaudio=1\nspaudioeffectver=5\nspau=284\nspuhdsfrhdr=<NULL>\n";
inline const char* CAPS_UHD_NO_VVC   = "drm=1032\nsptxvdrm=63\nspvideo=1028\ndolbydecode=40\nsphls=14\nspm3u8tag=19027\nhevclv=33\nspavs3=15\nsphevcfps=70368744177663\nspvvcfps=0\nspsfrhdr=300\nspaudio=3471\nspaudioeffectver=5\nspau=284\nspuhdsfrhdr=200\n";
inline const char* CAPS_NO_DRM       = "drm=0\nsptxvdrm=0\nspvideo=5124\ndolbydecode=40\nsphls=14\nspm3u8tag=19027\nhevclv=33\nspavs3=15\nsphevcfps=70368744177663\nspvvcfps=47\nspsfrhdr=300\nspaudio=3471\nspaudioeffectver=5\nspau=284\nspuhdsfrhdr=200\n";
inline const char* CAPS_HDR_UHDOFF   = "drm=1032\nsptxvdrm=63\nspvideo=5124\ndolbydecode=40\nsphls=14\nspm3u8tag=19027\nhevclv=33\nspavs3=15\nsphevcfps=70368744177663\nspvvcfps=47\nspsfrhdr=250\nspaudio=3471\nspaudioeffectver=5\nspau=284\nspuhdsfrhdr=<NULL>\n";
inline const char* CONST_SAFE_STATIC = "spaudio=3471\nspaudioeffectver=5\nspau=284\ndolbydecode=40\nspvideo=5124\nspvfps=240\nsphevcfps=70368744177663\nspvvcfps=47\nspavs3=15\nspsfrhdr=300\nspuhdsfrhdr=200\nsphls=14\nspm3u8tag=19027\nspmasterm3u8=19\nspcaptiontrack=1\nspcaptiontype=1\nspsrt=3\nsprichtype=all\nsprichver=2.1.0\nspadseg=3\nspwm=8\nspvvpay=0\nsppreviewtype=1\nssv=13\nsptxvdrm=63\nhevclv=33\nspptype=4,5,6,7,8,9,10,11,12,101\nencryptVer=4.2\ndefnpayver=1\notype=ojson\ndtype=3\nspgzip=1\nplayctrl=274\nappctrl=507905\nsubctrl=2\nlivequeue=1\nadversion=220220\nadchid=0\nplat_bucketid=0\ndrm=1032\ndefn=uhd\ndefnsrc=3\ndefnswitch=0\nclip=4\nretry=0\nscene=playerUiType=0\ndsextinfo=sourcetype=0&abtest=-1\nplatform=10303\nsdtfrom=v5000\nappVer=9.02.90.31109\nmanufacturer=samsung\ndevicemodel=SM-G991N\nosver=14\nscreeninfo=1080*2176\nqq=\nsrccontenid=\n";
inline const char* CONST_PATH_NORMAL = "costdivide=videoType%3D2\n";
inline const char* CONST_PATH_HISTORY= "previd_type=0\n";

// ── Pha 4: getvinfo_params ────────────────────────────────────────────────────
// flowid
inline const char* FLOWID_UUID       = "12345678-1234-4234-8234-123456789abc";
inline const uint64_t FLOWID_NANO    = 46886803916039ull;
inline const char* FLOWID_PLATFORM   = "10303";
inline const char* FLOWID_EXPECTED   = "12345678-1234-4234-8234-123456789abc46886803916039_10303";
inline const uint64_t FLOWID_NANO2   = 42ull;
inline const char* FLOWID_EXPECTED2  = "12345678-1234-4234-8234-123456789abc00000000000042_10303";
// encode_body (URL-encode kiểu Java)
inline const char* ENCBODY_EXPECTED  = "a=hello%20world&b=x%3D1%26y%3D2&c=1080*2176&d=a%2Fb%3Ac&e=caf%C3%A9&f=~tilde&g=100%25done&h=";
// generate_ckey tất định (fixture ckey, inject param2/rand21/tv_usec/pad)
inline const char* CKEY_GEN_PARAM2_HEX = "3137383034373033353131313661616161616161616161616161616161";
inline const char* CKEY_GEN_RAND21_HEX = "62626262626262626262626262626262";
inline const char* CKEY_GEN_DD_RANDOM8_HEX = "c0c1c2c3c4c5c6c7";
inline const uint32_t CKEY_GEN_TVUSEC  = 656810u;
inline const char* CKEY_GEN_RAND2_HEX  = "c8c9";
inline const uint8_t CKEY_GEN_HEAD1    = 0xcau;
inline const char* CKEY_GEN_VID        = "TESTVID123";
inline const char* CKEY_GEN_EXPECTED   = "--01De3uWYza5mxP96sVOXBB0jUJqZyn8tB8U55obOpitxjqXtd2QgIm-j5pozR2kRC8y7ze5zrk_8dQNoaMZISFG1Q2F5t5byf20jRmvYJnCZ8BRiTiXEd5BcaiM8rEZbPHg-YEe2ldJQHzHogTL-E74hK0BnNv9Z6F7mYIe3sGzUtmah956YQGR6LIUwZFcCBtmG6QVwDNZUniIcGFV-oeiciIFYCcWpPUCav7F0yABZxMiRCoXCPQQfEdm1zz0aBPGRHUlRXuWsWpNeN5DTgCcb8NesUE5IgnoEay-PHXJELlVXRI72P8uP-Xx8dFmvsOAG9XwLzl05oPQa7hS4VVhgwHRpGHIKSXUb1oYYi-vLY2rCiIrYo7HDev-LQrsqtJR1y2n5Rk6YTBqPifobb4JXC3_t8RGwuexL-U7I1BDJ3N28MALXxvpjG7ZsKDgVe2y8y-Jj2qDkvTZ0Tr39uCn5lSinntWv1r88S7fS8HCcsImIZZGYL0RoFP8A_WSjlvd6D4-fxx5rbZporPGH6zeEf7Pf-CE_ap3wECl0HkBYnqekqrdNKl10hv5GuuKSieAR6Tzy7xSryJDJbDmfnAWOEt1ozpauFHmcNbfgdUDb9aP8uCsKCWmd8nX3s_ZzPk7k3snPF_p35MCI7rSRkDxRldPabti3eZFxSU8x4-APo_EV7uLE8VCDiiGj7DULtQC53qdZWo40Pta5Xke9VVuTnSrfnz28Uyz4hZvHIRlpRVKWtcAkqllUfSr4BnXtrvii_EHwyXlOB_R7fSYzXBwE6-7576o0QYTAiEjb-3fBssypsHOe5I2qJTMneKSoROz7PQPzi7ZxnYiNozwcJ73hilSA7IWiqPxxw05dqCiHAJJRrTSrV2rvN8r_I2bLlbwsVEpKq_yyvCsqDcDPNaDbAEzq9TA8Yz0jy8cgdOEO0Mka8Pd7Jo0y1C0HQ";
// build_query_params (flowid/ckey cố định)
inline const char* BUILD_FIXED_FLOW    = "FLOWFIXED123_10303";
inline const char* BUILD_FIXED_CKEY    = "--01CKEYFIXED";
inline const char* BUILD_NORMAL        = "vid=NEWVID\ncid=NEWCID\nguid=GUID123\nqimei=QIMEI36\nabuserid=AB123\nuserid=U1\nopenid=O1\nviptype=1\nuser_level=2\ndetail_user_level=3\nrec_user_level=4\nadpass=%7B%22shouldSkipCustomAdWithoutLogin%22%3Afalse%2C%22interestAdTags%22%3A%22%22%2C%22adCookie%22%3A%22video_platform%3D2%3Bmain_login%3Dwx%3Bctime%3D123%3B%22%2C%22taidTicket%22%3A%22%22%2C%22ad_qq_openid%22%3A%22%22%2C%22mnt_id%22%3A%22%22%2C%22device_init_time%22%3A%22%22%2C%22ad_experiment_id%22%3A%220%22%2C%22encryptedOaid%22%3A%22%22%2C%22isPreviewMode%22%3Afalse%2C%22skp_mode%22%3A%220%22%2C%22is_blind_authorize%22%3A%220%22%2C%22wxVersionCode%22%3A671106370%2C%22osVersion%22%3A%22Android_14%22%2C%22is_free_mode_user%22%3Afalse%2C%22free_mode_user_has_permission%22%3Afalse%7D\nplatform=10303\nsdtfrom=v5000\nencryptVer=4.2\nspvideo=7777\ndefn=uhd\ndefnswitch=0\nnewnettype=1\nipstack=3\nbitrate=222\natime=111\nmediascene=fwexpid=&fwtype=2&ctime=0&pluginadctrl=1\nreportparams=orispvvcfps=47&abtestid=&fwexpid=&spvvcfpsreasonnew=0&fwtype=2\ncostdivide=videoType%3D2\nscene=playerUiType=0\nflowid=FLOWFIXED123_10303\ncKey=--01CKEYFIXED\nspaudio=3471\nspaudioeffectver=5\nspau=284\ndolbydecode=40\nspvfps=240\nsphevcfps=70368744177663\nspvvcfps=47\nspavs3=15\nspsfrhdr=300\nspuhdsfrhdr=200\nsphls=14\nspm3u8tag=19027\nspmasterm3u8=19\nspcaptiontrack=1\nspcaptiontype=1\nspsrt=3\nsprichtype=all\nsprichver=2.1.0\nspadseg=3\nspwm=8\nspvvpay=0\nsppreviewtype=1\nssv=13\nsptxvdrm=63\nhevclv=33\nspptype=4,5,6,7,8,9,10,11,12,101\ndefnpayver=1\notype=ojson\ndtype=3\nspgzip=1\nplayctrl=274\nappctrl=507905\nsubctrl=2\nlivequeue=1\nadversion=220220\nadchid=0\nplat_bucketid=0\ndefnsrc=3\nclip=4\nretry=0\ndsextinfo=sourcetype=0&abtest=-1\nappVer=9.02.90.31109\nmanufacturer=samsung\ndevicemodel=SM-G991N\nosver=14\nscreeninfo=1080*2176\nqq=\nsrccontenid=\nextraNew=E1\n";
inline const char* BUILD_NORMAL_BODY   = "vid=NEWVID&cid=NEWCID&guid=GUID123&qimei=QIMEI36&abuserid=AB123&userid=U1&openid=O1&viptype=1&user_level=2&detail_user_level=3&rec_user_level=4&adpass=%257B%2522shouldSkipCustomAdWithoutLogin%2522%253Afalse%252C%2522interestAdTags%2522%253A%2522%2522%252C%2522adCookie%2522%253A%2522video_platform%253D2%253Bmain_login%253Dwx%253Bctime%253D123%253B%2522%252C%2522taidTicket%2522%253A%2522%2522%252C%2522ad_qq_openid%2522%253A%2522%2522%252C%2522mnt_id%2522%253A%2522%2522%252C%2522device_init_time%2522%253A%2522%2522%252C%2522ad_experiment_id%2522%253A%25220%2522%252C%2522encryptedOaid%2522%253A%2522%2522%252C%2522isPreviewMode%2522%253Afalse%252C%2522skp_mode%2522%253A%25220%2522%252C%2522is_blind_authorize%2522%253A%25220%2522%252C%2522wxVersionCode%2522%253A671106370%252C%2522osVersion%2522%253A%2522Android_14%2522%252C%2522is_free_mode_user%2522%253Afalse%252C%2522free_mode_user_has_permission%2522%253Afalse%257D&platform=10303&sdtfrom=v5000&encryptVer=4.2&spvideo=7777&defn=uhd&defnswitch=0&newnettype=1&ipstack=3&bitrate=222&atime=111&mediascene=fwexpid%3D%26fwtype%3D2%26ctime%3D0%26pluginadctrl%3D1&reportparams=orispvvcfps%3D47%26abtestid%3D%26fwexpid%3D%26spvvcfpsreasonnew%3D0%26fwtype%3D2&costdivide=videoType%253D2&scene=playerUiType%3D0&flowid=FLOWFIXED123_10303&cKey=--01CKEYFIXED&spaudio=3471&spaudioeffectver=5&spau=284&dolbydecode=40&spvfps=240&sphevcfps=70368744177663&spvvcfps=47&spavs3=15&spsfrhdr=300&spuhdsfrhdr=200&sphls=14&spm3u8tag=19027&spmasterm3u8=19&spcaptiontrack=1&spcaptiontype=1&spsrt=3&sprichtype=all&sprichver=2.1.0&spadseg=3&spwm=8&spvvpay=0&sppreviewtype=1&ssv=13&sptxvdrm=63&hevclv=33&spptype=4%2C5%2C6%2C7%2C8%2C9%2C10%2C11%2C12%2C101&defnpayver=1&otype=ojson&dtype=3&spgzip=1&playctrl=274&appctrl=507905&subctrl=2&livequeue=1&adversion=220220&adchid=0&plat_bucketid=0&defnsrc=3&clip=4&retry=0&dsextinfo=sourcetype%3D0%26abtest%3D-1&appVer=9.02.90.31109&manufacturer=samsung&devicemodel=SM-G991N&osver=14&screeninfo=1080*2176&qq=&srccontenid=&extraNew=E1";
inline const char* BUILD_HISTORY       = "vid=\ncid=oldcid\nguid=GUID123\nqimei=QIMEI36\nabuserid=AB123\nuserid=U1\nopenid=O1\nviptype=1\nuser_level=2\ndetail_user_level=3\nrec_user_level=4\nadpass=%7B%22shouldSkipCustomAdWithoutLogin%22%3Afalse%2C%22interestAdTags%22%3A%22%22%2C%22adCookie%22%3A%22video_platform%3D2%3Bmain_login%3Dwx%3Bctime%3D123%3B%22%2C%22taidTicket%22%3A%22%22%2C%22ad_qq_openid%22%3A%22%22%2C%22mnt_id%22%3A%22%22%2C%22device_init_time%22%3A%22%22%2C%22ad_experiment_id%22%3A%220%22%2C%22encryptedOaid%22%3A%22%22%2C%22isPreviewMode%22%3Afalse%2C%22skp_mode%22%3A%220%22%2C%22is_blind_authorize%22%3A%220%22%2C%22wxVersionCode%22%3A671106370%2C%22osVersion%22%3A%22Android_14%22%2C%22is_free_mode_user%22%3Afalse%2C%22free_mode_user_has_permission%22%3Afalse%7D\nplatform=10303\nsdtfrom=v5000\nencryptVer=4.2\nspvideo=5124\ndrm=1032\ndefn=hd\ndefnswitch=0\nnewnettype=1\nipstack=3\nbitrate=5000\natime=0\nmediascene=fwexpid=&fwtype=2&ctime=0&pluginadctrl=1\nreportparams=orispvvcfps=47&abtestid=&fwexpid=&spvvcfpsreasonnew=0&fwtype=2\nprevid={\"h\":\"1\"}\nprevid_type=0\nscene=playerUiType=0\nflowid=FLOWFIXED123_10303\ncKey=--01CKEYFIXED\nspaudio=3471\nspaudioeffectver=5\nspau=284\ndolbydecode=40\nspvfps=240\nsphevcfps=70368744177663\nspvvcfps=47\nspavs3=15\nspsfrhdr=300\nspuhdsfrhdr=200\nsphls=14\nspm3u8tag=19027\nspmasterm3u8=19\nspcaptiontrack=1\nspcaptiontype=1\nspsrt=3\nsprichtype=all\nsprichver=2.1.0\nspadseg=3\nspwm=8\nspvvpay=0\nsppreviewtype=1\nssv=13\nsptxvdrm=63\nhevclv=33\nspptype=4,5,6,7,8,9,10,11,12,101\ndefnpayver=1\notype=ojson\ndtype=3\nspgzip=1\nplayctrl=274\nappctrl=507905\nsubctrl=2\nlivequeue=1\nadversion=220220\nadchid=0\nplat_bucketid=0\ndefnsrc=3\nclip=4\nretry=0\ndsextinfo=sourcetype=0&abtest=-1\nappVer=9.02.90.31109\nmanufacturer=samsung\ndevicemodel=SM-G991N\nosver=14\nscreeninfo=1080*2176\nqq=\nsrccontenid=\n";

}  // namespace vec
