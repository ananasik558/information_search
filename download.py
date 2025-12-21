# download_en_articles.py
import os
import requests
from bs4 import BeautifulSoup
import time
import urllib.parse
import json
import hashlib

CORPUS_DIR = "corpus_en"
TITLES_FILE = "data/titles_en.txt"
STATE_FILE = "crawler_state.json"
MAX_DOCS = 50000
REQUEST_DELAY = 1.5
USER_AGENT = "MAI-CarCrawler/1.0"


def extract_text(html: str) -> str:
    soup = BeautifulSoup(html, "html.parser")
    for tag in soup(["script", "style", "nav", "footer", "aside", "table.infobox", "div.navbox", "div.thumb", "div.metadata"]):
        tag.decompose()
    content = soup.find("div", {"class": "mw-parser-output"})
    if content:
        return content.get_text(separator=" ", strip=True)
    return ""


def load_state():
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            print(f"Ошибка загрузки состояния: {e}")
    return {
        "last_processed_title": "",
        "total_downloaded": 0
    }


def save_state(state):
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, ensure_ascii=False, indent=2)


def get_meta_path(filepath: str) -> str:
    return filepath + ".meta.json"


def fetch_document(url: str):
    try:
        headers = {"User-Agent": USER_AGENT}
        resp = requests.get(url, headers=headers, timeout=20)
        if resp.status_code == 200:
            return resp.text, resp.headers
    except Exception as e:
        print(f"Ошибка загрузки: {e}")
    return None, None


def main():
    os.makedirs(CORPUS_DIR, exist_ok=True)
    state = load_state()

    with open(TITLES_FILE, "r", encoding="utf-8") as f:
        titles = [line.strip() for line in f if line.strip()]
    print(f"Найдено заголовков: {len(titles)}")

    start_index = 0
    if state["last_processed_title"]:
        try:
            start_index = titles.index(state["last_processed_title"]) + 1
            print(f"Возобновляем с индекса {start_index}")
        except ValueError:
            print("Последний заголовок не найден — начинаем с начала")

    saved = state["total_downloaded"]

    for i, title in enumerate(titles[start_index:], start=start_index):
        if saved >= MAX_DOCS:
            break

        safe_title = "".join(c if c.isalnum() or c in "._- " else "_" for c in title)[:80]
        base_filename = f"{saved:05d}_{safe_title}"
        txt_path = os.path.join(CORPUS_DIR, base_filename + ".txt")
        meta_path = get_meta_path(txt_path)

        encoded_title = urllib.parse.quote(title)
        url = f"https://en.wikipedia.org/wiki/{encoded_title}"

        print(f"[{i+1}/{len(titles)}] {title}")

        need_fetch = True
        if os.path.exists(meta_path):
            try:
                with open(meta_path, "r", encoding="utf-8") as f:
                    meta = json.load(f)
                last_mod_stored = meta.get("last_modified")
                etag_stored = meta.get("etag")
                head_resp = requests.head(url, headers={"User-Agent": USER_AGENT}, timeout=10)
                if head_resp.status_code != 200:
                    continue
                last_mod_current = head_resp.headers.get("Last-Modified")
                etag_current = head_resp.headers.get("ETag")

                if last_mod_stored == last_mod_current:
                    print("  Не изменился (Last-Modified)")
                    need_fetch = False
                elif etag_stored == etag_current:
                    print("  Не изменился (ETag)")
                    need_fetch = False
            except Exception as e:
                print(f"Ошибка при проверке метаданных: {e}")

        if not need_fetch:
            saved += 1 
            state["last_processed_title"] = title
            state["total_downloaded"] = saved
            save_state(state)
            continue

        html, headers = fetch_document(url)
        if html is None:
            continue

        text = extract_text(html)
        if len(text.split()) < 100:
            continue

        with open(txt_path, "w", encoding="utf-8") as f:
            f.write(text)

        content_hash = hashlib.md5(text.encode("utf-8")).hexdigest()
        meta = {
            "url": url,
            "title": title,
            "fetch_time": int(time.time()),
            "last_modified": headers.get("Last-Modified"),
            "etag": headers.get("ETag"),
            "content_hash": content_hash
        }
        with open(meta_path, "w", encoding="utf-8") as f:
            json.dump(meta, f, ensure_ascii=False, indent=2)

        saved += 1
        state["last_processed_title"] = title
        state["total_downloaded"] = saved
        save_state(state)
        print(f"Сохранено ({saved})")

        time.sleep(REQUEST_DELAY)

    print(f"\nЗавершено. Всего документов: {saved}")


if __name__ == "__main__":
    main()