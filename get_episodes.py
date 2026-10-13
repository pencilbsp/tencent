#!/usr/bin/env python3
"""
Tencent Video Episode Fetcher
Fetches episode list, VIDs, and metadata from Tencent Video (v.qq.com)
"""

import argparse
import json
import os
import random
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Any, Dict, List, Optional, Tuple

API_BASE = "https://pbaccess.video.qq.com/trpc.vector_layout.page_view.PageService/getPage"
APP_ID = "3000010"
VERSION_NAME = "8.5.96"
PLATFORM = "2"

HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/131.0.0.0 Safari/537.36"
    ),
    "Referer": "https://v.qq.com/",
    "Origin": "https://v.qq.com",
    "Content-Type": "application/json",
    "Accept": "application/json",
}


def generate_guid() -> str:
    """Generate 16-hex-digit pseudo GUID matching frontend implementation."""
    return "".join(f"{random.randint(0, 0xffff):04x}" for _ in range(4))


def parse_target(target: str) -> Tuple[str, str]:
    """
    Extract (cid, vid) from a URL or raw ID string.
    Supported inputs:
      - https://v.qq.com/x/cover/mzc00200kwy29cd/f0043di9m10.html
      - https://v.qq.com/x/cover/mzc00200kwy29cd.html
      - https://v.qq.com/x/page/f0043di9m10.html
      - https://m.v.qq.com/x/m/play?cid=mzc00200kwy29cd&vid=f0043di9m10
      - mzc00200kwy29cd
      - f0043di9m10
    """
    target = target.strip()
    cid = ""
    vid = ""

    # Check for query params (like mobile URL)
    if "?" in target:
        parsed_url = urllib.parse.urlparse(target)
        qs = urllib.parse.parse_qs(parsed_url.query)
        if "cid" in qs and qs["cid"]:
            cid = qs["cid"][0]
        if "vid" in qs and qs["vid"]:
            vid = qs["vid"][0]
        if cid or vid:
            return cid, vid

    # Cover with VID pattern: /x/cover(?:_seo)?/(?:[a-z0-9]/)?(<cid>)/(<vid>).html
    m_cover_vid = re.search(r"/x/cover(?:_seo)?/(?:[a-zA-Z0-9]/)?([a-zA-Z0-9]{15,16})/([a-zA-Z0-9]{11})\.html", target)
    if m_cover_vid:
        return m_cover_vid.group(1), m_cover_vid.group(2)

    # Cover only pattern: /x/cover(?:_seo)?/(?:[a-z0-9]/)?(<cid>).html
    m_cover = re.search(r"/x/cover(?:_seo)?/(?:[a-zA-Z0-9]/)?([a-zA-Z0-9]{15,16})\.html", target)
    if m_cover:
        return m_cover.group(1), ""

    # Page/VID only pattern: /x/page(?:_seo)?/(<vid>).html
    m_page = re.search(r"/x/page(?:_seo)?/([a-zA-Z0-9]{11})\.html", target)
    if m_page:
        return "", m_page.group(1)

    # Raw CID (starts with mzc or 15-16 chars)
    if re.match(r"^mzc[a-zA-Z0-9]{12,13}$", target) or len(target) in (15, 16):
        return target, ""

    # Raw VID (usually 11 chars)
    if len(target) == 11 and re.match(r"^[a-zA-Z0-9]{11}$", target):
        return "", target

    return target, ""


def fetch_main_page(cid: str, vid: str = "", guid: Optional[str] = None) -> Dict[str, Any]:
    """Fetch the main video detail page data using Tencent Vector Layout PageService."""
    if not guid:
        guid = generate_guid()

    params = {
        "video_appid": APP_ID,
        "vversion_platform": PLATFORM,
        "vversion_name": VERSION_NAME,
        "vdevice_guid": guid,
    }
    url = f"{API_BASE}?{urllib.parse.urlencode(params)}"

    payload = {
        "page_params": {
            "req_from": "web_vsite",
            "new_mark_label_enabled": "1",
            "cid": cid,
            "vid": vid,
            "lid": "",
            "is_pc_new_detail_page": "0",
            "is_from_web_flyflow": "1",
        },
        "page_bypass_params": {
            "params": {
                "caller_id": APP_ID,
                "platform_id": PLATFORM,
            },
            "scene": "desk_detail",
            "app_version": "",
            "abtest_bypass_id": guid,
        },
        "page_context": {},
    }

    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers=HEADERS,
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read().decode("utf-8"))


def fetch_sec_page(cid: str, page_context_str: str, page_id: str = "web_episode_list", guid: Optional[str] = None) -> Dict[str, Any]:
    """Fetch secondary page / episode range data using page_context."""
    if not guid:
        guid = generate_guid()

    params = {
        "video_appid": APP_ID,
        "vversion_platform": PLATFORM,
        "vversion_name": VERSION_NAME,
        "vdevice_guid": guid,
    }
    url = f"{API_BASE}?{urllib.parse.urlencode(params)}"

    payload = {
        "page_params": {
            "ad_wechat_authorization_status": "0",
            "req_from": "web_vsite",
            "ad_exp_ids": "",
            "pc_sdk_version": "",
            "pc_oaid": "",
            "new_mark_label_enabled": "1",
            "pc_device_info": "",
            "support_pc_yyb_mobile_app_engine": "0",
            "pc_wegame_version": "",
            "cid": cid,
            "vid": "",
            "page_id": page_id,
            "page_context": page_context_str,
            "page_type": "detail_operation",
            "is_pc_new_detail_page": "0",
            "is_from_web_flyflow": "1",
        },
        "page_bypass_params": {
            "params": {
                "caller_id": APP_ID,
                "platform_id": PLATFORM,
                "page_type": "detail_operation",
                "page_id": page_id,
                "data_mode": "default",
                "user_mode": "default",
                "new_mark_label_enabled": "1",
            },
            "scene": "operation",
            "app_version": "",
            "abtest_bypass_id": guid,
        },
        "page_context": {
            "latestPageContext": page_context_str,
        },
    }

    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers=HEADERS,
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read().decode("utf-8"))


def parse_introduction(card_list: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Extract show title, description, rating, and metadata."""
    info = {
        "title": "",
        "description": "",
        "year": "",
        "genres": "",
        "score": "",
        "total_episodes": "",
        "cid": "",
    }
    for card in card_list:
        if card.get("type") in ("pc_introduction", "introduction"):
            ch = card.get("children_list", {})
            for group in ch.values():
                for c in group.get("cards", []):
                    p = c.get("params", {})
                    if p.get("title") and not info["title"]:
                        info["title"] = p.get("title", "")
                    if p.get("cover_description") and not info["description"]:
                        info["description"] = p.get("cover_description", "")
                    if p.get("year") and not info["year"]:
                        info["year"] = p.get("year", "")
                    if p.get("main_genres") and not info["genres"]:
                        info["genres"] = p.get("main_genres", "")
                    if p.get("episode_all") and not info["total_episodes"]:
                        info["total_episodes"] = p.get("episode_all", "")
                    if p.get("report.cid") and not info["cid"]:
                        info["cid"] = p.get("report.cid", "")
                    score_json = p.get("score_info")
                    if score_json:
                        try:
                            score_data = json.loads(score_json)
                            if "video_score" in score_data:
                                info["score"] = score_data["video_score"]
                        except Exception:
                            pass
    return info


def format_duration(seconds_str: Optional[str]) -> str:
    """Format duration in seconds to MM:SS or HH:MM:SS."""
    if not seconds_str:
        return ""
    try:
        total_sec = int(seconds_str)
        hours = total_sec // 3600
        minutes = (total_sec % 3600) // 60
        secs = total_sec % 60
        if hours > 0:
            return f"{hours:02d}:{minutes:02d}:{secs:02d}"
        return f"{minutes:02d}:{secs:02d}"
    except (ValueError, TypeError):
        return str(seconds_str)


def parse_episodes_from_card(
    card: Dict[str, Any],
    include_trailers: bool = False,
) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]], List[Dict[str, Any]]]:
    """
    Extract episodes, range tabs, and season tabs from an episode card.
    Returns (episodes, range_tabs, season_tabs).
    """
    episodes: List[Dict[str, Any]] = []
    range_tabs: List[Dict[str, Any]] = []
    season_tabs: List[Dict[str, Any]] = []

    params = card.get("params", {})

    # Check for range tabs in params (e.g. 1-30, 31-60)
    tabs_str = params.get("tabs")
    if tabs_str:
        try:
            range_tabs = json.loads(tabs_str)
        except Exception:
            pass

    children = card.get("children_list", {})
    sorted_keys = sorted(children.keys(), key=lambda x: int(x) if x.isdigit() else 9999)

    for k in sorted_keys:
        for item in children[k].get("cards", []):
            p = item.get("params", {})
            item_type = p.get("card_render@item_type")

            # Check if this item is a season/playlist tab (item_type '28')
            if item_type == "28":
                tab_title = p.get("title", "")
                page_ctx = p.get("page_context", "")
                page_id = p.get("page_id", "web_episode_list")
                # Try to extract CID from page_context
                tab_cid = ""
                if page_ctx:
                    match_cid = re.search(r"cid=([a-zA-Z0-9]+)", page_ctx)
                    if match_cid:
                        tab_cid = match_cid.group(1)
                season_tabs.append({
                    "title": tab_title,
                    "cid": tab_cid,
                    "page_context": page_ctx,
                    "page_id": page_id,
                    "selected": p.get("selected") == "1",
                })
                continue

            vid = p.get("vid") or item.get("id")
            if not vid or len(vid) < 10:
                continue

            is_trailer = p.get("is_trailer") == "1"
            if is_trailer and not include_trailers:
                continue

            title = p.get("title") or p.get("c_title_output") or ""
            play_title = p.get("play_title") or p.get("union_title") or ""
            cid = p.get("cid", "")
            duration = p.get("duration", "")
            image_url = p.get("image_url", "")

            ep_url = f"https://v.qq.com/x/cover/{cid}/{vid}.html" if cid else f"https://v.qq.com/x/page/{vid}.html"

            episodes.append({
                "index": title,
                "vid": vid,
                "cid": cid,
                "title": title,
                "play_title": play_title,
                "duration": duration,
                "duration_formatted": format_duration(duration),
                "is_trailer": is_trailer,
                "url": ep_url,
                "image_url": image_url,
            })

    return episodes, range_tabs, season_tabs


def get_all_episodes(
    target: str,
    include_trailers: bool = False,
    guid: Optional[str] = None,
) -> Dict[str, Any]:
    """
    Fetch all episodes for a given URL, CID, or VID.
    Automatically fetches all pagination ranges/tabs.
    """
    if not guid:
        guid = generate_guid()

    cid, vid = parse_target(target)
    if not cid and not vid:
        raise ValueError(f"Could not parse a valid CID or VID from input: '{target}'")

    main_res = fetch_main_page(cid=cid, vid=vid, guid=guid)
    if main_res.get("ret", 0) != 0:
        msg = main_res.get("msg", "Unknown error")
        raise RuntimeError(f"Tencent API error (ret={main_res.get('ret')}): {msg}")

    data = main_res.get("data", {})
    card_list = data.get("CardList", [])

    info = parse_introduction(card_list)
    if not info["cid"] and cid:
        info["cid"] = cid

    # Find episode list card
    ep_card = None
    for card in card_list:
        if card.get("type") in ("pc_web_episode_list", "pc_detail_ep_list", "pc_sv_mixed_feeds", "pc_series"):
            ep_card = card
            break

    if not ep_card:
        # Fallback: check any card with episode-like children
        for card in card_list:
            if "episode" in card.get("type", "").lower() or card.get("params", {}).get("desk_module_type") == "episode_list":
                ep_card = card
                break

    if not ep_card:
        return {
            "info": info,
            "episodes": [],
            "range_tabs": [],
            "season_tabs": [],
            "raw_cards": len(card_list),
        }

    initial_episodes, range_tabs, season_tabs = parse_episodes_from_card(
        ep_card,
        include_trailers=include_trailers,
    )

    all_episodes: List[Dict[str, Any]] = []
    seen_vids = set()

    # If there are range tabs (e.g. 1-30, 31-60...), fetch all ranges
    if range_tabs and len(range_tabs) > 1:
        current_cid = info["cid"] or cid
        for tab in range_tabs:
            ctx = tab.get("page_context")
            if not ctx:
                continue
            try:
                sec_res = fetch_sec_page(current_cid, ctx, guid=guid)
                sec_cards = sec_res.get("data", {}).get("CardList", [])
                for sc in sec_cards:
                    sec_eps, _, _ = parse_episodes_from_card(sc, include_trailers=include_trailers)
                    for ep in sec_eps:
                        if ep["vid"] not in seen_vids:
                            seen_vids.add(ep["vid"])
                            all_episodes.append(ep)
            except Exception as e:
                print(f"[Warning] Failed to fetch range tab {tab.get('begin_text')}-{tab.get('end_text')}: {e}", file=sys.stderr)
    else:
        for ep in initial_episodes:
            if ep["vid"] not in seen_vids:
                seen_vids.add(ep["vid"])
                all_episodes.append(ep)

    return {
        "info": info,
        "episodes": all_episodes,
        "range_tabs": range_tabs,
        "season_tabs": season_tabs,
    }


def print_table(data: Dict[str, Any]):
    """Print episodes in a formatted table."""
    info = data.get("info", {})
    episodes = data.get("episodes", [])
    season_tabs = data.get("season_tabs", [])

    print("=" * 80)
    title = info.get("title") or "Tencent Video"
    print(f"🎬  {title}")
    if info.get("year") or info.get("genres") or info.get("score"):
        details = []
        if info.get("year"):
            details.append(f"Year: {info['year']}")
        if info.get("genres"):
            details.append(f"Genre: {info['genres']}")
        if info.get("score"):
            details.append(f"Score: {info['score']}")
        if info.get("total_episodes"):
            details.append(f"Total: {info['total_episodes']} eps")
        print(f"    {' | '.join(details)}")
    if info.get("cid"):
        print(f"    CID: {info['cid']}")
    if info.get("description"):
        desc = info["description"]
        if len(desc) > 100:
            desc = desc[:97] + "..."
        print(f"    Intro: {desc}")
    print("=" * 80)

    if season_tabs:
        seasons_str = " | ".join(
            f"[{s['title']} (CID: {s['cid']})]" if s.get("selected") else f"{s['title']} ({s['cid']})"
            for s in season_tabs
        )
        print(f"📺  Seasons / Playlists: {seasons_str}")
        print("-" * 80)

    if not episodes:
        print("⚠️  No episodes found.")
        return

    print(f"{'#':<6} {'VID':<15} {'Duration':<10} {'Title / Play Title'}")
    print("-" * 80)
    for i, ep in enumerate(episodes, 1):
        idx_str = ep.get("title") or str(i)
        vid = ep.get("vid", "")
        dur = ep.get("duration_formatted", "")
        play_title = ep.get("play_title") or ep.get("title") or ""
        trailer_tag = " [Trailer]" if ep.get("is_trailer") else ""
        print(f"{idx_str:<6} {vid:<15} {dur:<10} {play_title}{trailer_tag}")

    print("-" * 80)
    print(f"Total episodes found: {len(episodes)}")


def print_bash_array(episodes: List[Dict[str, Any]]):
    """Print episodes in bash array format for download scripts."""
    print("declare -a VIDS=(")
    for ep in episodes:
        vid = ep.get("vid", "")
        title = ep.get("play_title") or ep.get("title") or ""
        print(f'  "{vid}" # {title}')
    print(")")


def main():
    parser = argparse.ArgumentParser(
        description="Fetch episode list and VIDs from Tencent Video (v.qq.com)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 get_episodes.py https://v.qq.com/x/cover/mzc00200kwy29cd/f0043di9m10.html
  python3 get_episodes.py mzc00200kwy29cd
  python3 get_episodes.py mzc00200kwy29cd --bash
  python3 get_episodes.py mzc00200kwy29cd --vids
  python3 get_episodes.py mzc00200kwy29cd --json -o episodes.json
  python3 get_episodes.py mzc00200kwy29cd --all-seasons
        """,
    )
    parser.add_argument(
        "target",
        nargs="?",
        default="https://v.qq.com/x/cover/mzc00200kwy29cd/f0043di9m10.html",
        help="Tencent Video URL, CID, or VID (default: user requested URL)",
    )
    parser.add_argument(
        "--cid",
        help="Specify CID directly",
    )
    parser.add_argument(
        "--vid",
        help="Specify VID directly",
    )
    parser.add_argument(
        "--include-trailers",
        "-t",
        action="store_true",
        help="Include trailers and promotional clips in output",
    )
    parser.add_argument(
        "--bash",
        action="store_true",
        help="Output in bash array format suitable for download_tn.sh",
    )
    parser.add_argument(
        "--vids",
        "-v",
        action="store_true",
        help="Output list of VIDs only (one per line)",
    )
    parser.add_argument(
        "--urls",
        action="store_true",
        help="Output list of full episode URLs (one per line)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output full metadata in JSON format",
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Save output to specified file path",
    )
    parser.add_argument(
        "--all-seasons",
        action="store_true",
        help="Fetch episodes for all detected seasons/playlists",
    )

    args = parser.parse_args()

    target = args.target
    if args.cid:
        target = args.cid
        if args.vid:
            target = f"https://v.qq.com/x/cover/{args.cid}/{args.vid}.html"
    elif args.vid and not target:
        target = args.vid

    try:
        data = get_all_episodes(target, include_trailers=args.include_trailers)
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        sys.exit(1)

    if args.all_seasons and data.get("season_tabs"):
        season_tabs = data.get("season_tabs", [])
        all_season_data = []
        for s in season_tabs:
            s_cid = s.get("cid")
            if not s_cid:
                continue
            try:
                s_data = get_all_episodes(s_cid, include_trailers=args.include_trailers)
                all_season_data.append(s_data)
            except Exception as e:
                print(f"[Warning] Failed to fetch season {s.get('title')}: {e}", file=sys.stderr)

        if args.json:
            out_str = json.dumps(all_season_data, ensure_ascii=False, indent=2)
            if args.output:
                with open(args.output, "w", encoding="utf-8") as f:
                    f.write(out_str)
                print(f"Saved all seasons to {args.output}")
            else:
                print(out_str)
        else:
            for s_data in all_season_data:
                print_table(s_data)
                print()
        return

    # Single season output
    if args.json:
        out_str = json.dumps(data, ensure_ascii=False, indent=2)
        if args.output:
            with open(args.output, "w", encoding="utf-8") as f:
                f.write(out_str)
            print(f"Saved {len(data.get('episodes', []))} episodes to {args.output}")
        else:
            print(out_str)
    elif args.bash:
        if args.output:
            with open(args.output, "w", encoding="utf-8") as f:
                for ep in data.get("episodes", []):
                    f.write(f'  "{ep["vid"]}" # {ep.get("play_title") or ep.get("title")}\n')
            print(f"Saved bash array to {args.output}")
        else:
            print_bash_array(data.get("episodes", []))
    elif args.vids:
        if args.output:
            with open(args.output, "w", encoding="utf-8") as f:
                for ep in data.get("episodes", []):
                    f.write(f"{ep['vid']}\n")
            print(f"Saved VIDs to {args.output}")
        else:
            for ep in data.get("episodes", []):
                print(ep["vid"])
    elif args.urls:
        if args.output:
            with open(args.output, "w", encoding="utf-8") as f:
                for ep in data.get("episodes", []):
                    f.write(f"{ep['url']}\n")
            print(f"Saved URLs to {args.output}")
        else:
            for ep in data.get("episodes", []):
                print(ep["url"])
    else:
        print_table(data)
        if args.output:
            with open(args.output, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False, indent=2)
            print(f"\n[Saved full data to {args.output}]")


if __name__ == "__main__":
    main()
