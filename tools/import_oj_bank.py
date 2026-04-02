#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Import a subset of problems from a big OJ题库 RAR into this project's file-based question format.

Why subset only?
- This OJ loads every question's desc/header/tail into memory at startup.
- Importing tens of thousands would make startup extremely slow and memory-heavy.

Default strategy:
- Stream HTML from RAR via `7z x -so` (no full extraction).
- Parse sections (Description/Input/Output/Sample Input/Sample Output/Hint).
- Generate desc.txt + a stdin/stdout style Solve skeleton + sample-based test harness.
- Print `questions.list` lines for appending.

Example:
  python3 tools/import_oj_bank.py \
    --archive /root/oj/0972f-main/0972f-main/OJ题库.rar \
    --source DnuiOJ --start 1000 --count 200 \
    --questions-dir oj_server/questions \
    --write --print-list
"""

from __future__ import annotations

import argparse
import html as htmllib
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


@dataclass
class Problem:
    number: str
    title: str
    time_limit_s: int = 1
    mem_limit_kb: int = 30000
    star: str = "简单"
    desc: str = ""
    sample_in: str = ""
    sample_out: str = ""


SECTION_ALIASES = {
    "description": "Description",
    "input": "Input",
    "output": "Output",
    "sample input": "Sample Input",
    "sample output": "Sample Output",
    "hint": "HINT",
}


def run_7z_list(archive: Path) -> List[str]:
    cp = subprocess.run(
        ["7z", "l", "-ba", str(archive)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    paths: List[str] = []
    for line in cp.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        # `7z l -ba` outputs fixed columns, last token is the path.
        parts = line.split()
        if parts:
            paths.append(parts[-1])
    return paths


def run_7z_cat(archive: Path, inner_path: str) -> str:
    # Stream file content from archive.
    cp = subprocess.run(
        ["7z", "x", "-so", str(archive), inner_path],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors="replace",
    )
    return cp.stdout


_tag_re = re.compile(r"<[^>]+>")


def html_to_text(s: str) -> str:
    # Preserve basic line breaks.
    s = re.sub(r"<\s*br\s*/?>", "\n", s, flags=re.IGNORECASE)
    s = re.sub(r"</\s*p\s*>", "\n", s, flags=re.IGNORECASE)
    s = re.sub(r"</\s*div\s*>", "\n", s, flags=re.IGNORECASE)
    s = _tag_re.sub("", s)
    s = htmllib.unescape(s)
    # Normalize whitespace while keeping newlines.
    s = s.replace("\r\n", "\n").replace("\r", "\n")
    s = re.sub(r"[ \t]+", " ", s)
    s = re.sub(r"\n{3,}", "\n\n", s)
    return s.strip()


def extract_main(html: str) -> str:
    m = re.search(r"<!--StartMarkForVirtualJudge-->(.*?)<!--EndMarkForVirtualJudge-->", html, flags=re.DOTALL)
    if m:
        return m.group(1)
    return html


def parse_limits(html: str) -> Tuple[Optional[int], Optional[int]]:
    # Time Limit: 1 Sec
    t: Optional[int] = None
    mm: Optional[int] = None

    mt = re.search(r"Time Limit:\s*</span>\s*([0-9]+)\s*Sec", html, flags=re.IGNORECASE)
    if mt:
        try:
            t = int(mt.group(1))
        except ValueError:
            t = None

    # Memory Limit: 128 MB
    mmatch = re.search(r"Memory Limit:\s*</span>\s*([0-9]+)\s*MB", html, flags=re.IGNORECASE)
    if mmatch:
        try:
            mm = int(mmatch.group(1)) * 1024
        except ValueError:
            mm = None

    return t, mm


def parse_title_and_number(html: str, fallback_number: str) -> Tuple[str, str]:
    # Prefer <title>1000: A+B Problem</title>
    m = re.search(r"<title>\s*(\d+)\s*:\s*(.*?)\s*</title>", html, flags=re.IGNORECASE | re.DOTALL)
    if m:
        return m.group(1).strip(), html_to_text(m.group(2))
    # Fallback to <h2>1000: ...</h2>
    m2 = re.search(r"<h2>\s*(\d+)\s*:\s*(.*?)\s*</h2>", html, flags=re.IGNORECASE | re.DOTALL)
    if m2:
        return m2.group(1).strip(), html_to_text(m2.group(2))
    return fallback_number, fallback_number


def parse_sections(main_html: str) -> Dict[str, str]:
    # Split by <h2>Section</h2>
    chunks: List[Tuple[str, str]] = []
    for m in re.finditer(r"<h2>\s*([^<]+?)\s*</h2>", main_html, flags=re.IGNORECASE):
        chunks.append((m.group(1).strip(), ""))
    if not chunks:
        return {}

    # Build ranges between h2 tags.
    sections: Dict[str, str] = {}
    h2_iter = list(re.finditer(r"<h2>\s*([^<]+?)\s*</h2>", main_html, flags=re.IGNORECASE))
    for idx, m in enumerate(h2_iter):
        name = m.group(1).strip()
        start = m.end()
        end = h2_iter[idx + 1].start() if idx + 1 < len(h2_iter) else len(main_html)
        body = main_html[start:end]
        sections[name] = body
    return sections


def extract_sample(section_html: str) -> str:
    # Typical: <pre class=content><span class=sampledata>...</span></pre>
    m = re.search(r"<span\s+class=sampledata>(.*?)</span>", section_html, flags=re.DOTALL | re.IGNORECASE)
    if m:
        return html_to_text(m.group(1))
    # Fallback to plain <pre>
    m2 = re.search(r"<pre[^>]*>(.*?)</pre>", section_html, flags=re.DOTALL | re.IGNORECASE)
    if m2:
        return html_to_text(m2.group(1))
    return html_to_text(section_html)


def sanitize_title_for_list(title: str) -> str:
    # questions.list uses space as separator; title must not contain spaces.
    t = re.sub(r"\s+", "_", title.strip())
    t = re.sub(r"[^0-9A-Za-z_\u4e00-\u9fff]+", "_", t)
    t = re.sub(r"_+", "_", t)
    return t[:60] if t else "Untitled"


def cpp_raw_string_literal(s: str, tag: str) -> str:
    # Choose a delimiter that doesn't appear in the string.
    delim = f"OJ_{tag}"
    while f"){delim}\"" in s:
        delim += "X"
    return f"R\"{delim}({s}){delim}\""


def generate_desc(p: Problem) -> str:
    parts: List[str] = []
    parts.append(f"{p.number}: {p.title}")
    parts.append("")
    parts.append("【Description】")
    parts.append(p.desc.strip() if p.desc.strip() else "(No description parsed)")
    parts.append("")
    if p.sample_in.strip() or p.sample_out.strip():
        parts.append("【Sample Input】")
        parts.append(p.sample_in.rstrip())
        parts.append("")
        parts.append("【Sample Output】")
        parts.append(p.sample_out.rstrip())
        parts.append("")
    parts.append("提示：本题使用‘样例判题’（仅对比题面样例输出）。")
    return "\n".join(parts).rstrip() + "\n"


def generate_header() -> str:
    return (
        "#include <bits/stdc++.h>\n"
        "using namespace std;\n\n"
        "// 约定：实现 Solve(in, out)，从 in 读取输入，向 out 输出结果。\n"
        "static void Solve(istream& in, ostream& out)\n"
        "{\n"
        "    // 请在此处编写你的代码\n"
        "    // TODO\n"
        "}\n"
    )


def generate_tail(p: Problem) -> str:
    sample_in = p.sample_in.replace("\\r\\n", "\\n").replace("\\r", "\\n")
    sample_out = p.sample_out.replace("\\r\\n", "\\n").replace("\\r", "\\n")

    inp_lit = cpp_raw_string_literal(sample_in, "IN")
    out_lit = cpp_raw_string_literal(sample_out, "OUT")

    return (
        "#ifndef COMPILER_ONLINE\n"
        "#include \"header.cpp\"\n"
        "#endif\n\n"
        "static string Normalize(string s)\n"
        "{\n"
        "    // normalize newlines\n"
        "    for (size_t i = 0; i + 1 < s.size(); i++)\n"
        "        if (s[i] == '\\r' && s[i + 1] == '\\n') { s.erase(i, 1); }\n"
        "    // trim trailing spaces per line\n"
        "    string out;\n"
        "    out.reserve(s.size());\n"
        "    size_t i = 0;\n"
        "    while (i < s.size())\n"
        "    {\n"
        "        size_t j = s.find('\\n', i);\n"
        "        if (j == string::npos) j = s.size();\n"
        "        size_t end = j;\n"
        "        while (end > i && (s[end - 1] == ' ' || s[end - 1] == '\\t')) end--;\n"
        "        out.append(s.substr(i, end - i));\n"
        "        if (j < s.size()) out.push_back('\\n');\n"
        "        i = j + 1;\n"
        "    }\n"
        "    // trim final newlines\n"
        "    while (!out.empty() && out.back() == '\\n') out.pop_back();\n"
        "    return out;\n"
        "}\n\n"
        "static string RunOne(const string& input)\n"
        "{\n"
        "    istringstream iss(input);\n"
        "    ostringstream oss;\n"
        "    Solve(iss, oss);\n"
        "    return oss.str();\n"
        "}\n\n"
        "static void Test1()\n"
        "{\n"
        f"    const string input = {inp_lit};\n"
        f"    const string expect = {out_lit};\n"
        "    const string got = RunOne(input);\n"
        "    if (Normalize(got) == Normalize(expect))\n"
        "    {\n"
        "        cout << \"通过用例1, 样例通过 ... OK!\" << endl;\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        cout << \"没有通过用例1, 样例不匹配\" << endl;\n"
        "        cout << \"[expect]\\n\" << expect << endl;\n"
        "        cout << \"[got]\\n\" << got << endl;\n"
        "    }\n"
        "}\n\n"
        "int main()\n"
        "{\n"
        "    Test1();\n"
        "    return 0;\n"
        "}\n"
    )


def parse_problem(html: str, fallback_number: str) -> Problem:
    number, title = parse_title_and_number(html, fallback_number)
    tl, ml = parse_limits(html)

    main = extract_main(html)
    sections = parse_sections(main)

    def pick(name: str) -> str:
        # Find section ignoring case and aliases.
        for k, body in sections.items():
            if k.strip().lower() == name.lower():
                return body
        # DnuiOJ uses HINT uppercase.
        for k, body in sections.items():
            if k.strip().lower() == name.lower().replace("hint", "h i n t").replace(" ", ""):
                return body
        return ""

    desc_html = pick("Description")
    in_html = pick("Input")
    out_html = pick("Output")
    sin_html = pick("Sample Input")
    sout_html = pick("Sample Output")
    hint_html = pick("HINT")

    # Build plain description combining sections (excluding samples).
    desc_parts: List[str] = []
    if desc_html.strip():
        desc_parts.append("Description:\n" + html_to_text(desc_html))
    if in_html.strip():
        desc_parts.append("Input:\n" + html_to_text(in_html))
    if out_html.strip():
        desc_parts.append("Output:\n" + html_to_text(out_html))
    if hint_html.strip():
        desc_parts.append("HINT:\n" + html_to_text(hint_html))

    p = Problem(
        number=number,
        title=title,
        time_limit_s=tl or 1,
        mem_limit_kb=ml or 30000,
        star="简单",
        desc="\n\n".join(desc_parts).strip(),
        sample_in=extract_sample(sin_html) if sin_html else "",
        sample_out=extract_sample(sout_html) if sout_html else "",
    )

    # If no sample output, keep empty and still generate runnable harness.
    return p


def ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)


def write_problem(out_dir: Path, p: Problem, overwrite: bool) -> None:
    ensure_dir(out_dir)

    def write_file(name: str, content: str) -> None:
        fp = out_dir / name
        if fp.exists() and not overwrite:
            return
        fp.write_text(content, encoding="utf-8")

    write_file("desc.txt", generate_desc(p))
    write_file("header.cpp", generate_header())
    write_file("tail.cpp", generate_tail(p))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--archive", required=True, type=Path)
    ap.add_argument("--source", default="DnuiOJ", help="Second-level dir under OJ题库/ (e.g. DnuiOJ, 洛谷)")
    ap.add_argument("--start", type=int, default=1000)
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--questions-dir", type=Path, default=Path("oj_server/questions"))
    ap.add_argument("--max-cpu", type=int, default=3, help="Clamp parsed cpu_limit to this maximum (seconds)")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--overwrite", action="store_true")
    ap.add_argument("--print-list", action="store_true")

    args = ap.parse_args()

    if not args.archive.exists():
        print(f"archive not found: {args.archive}", file=sys.stderr)
        return 2

    # Collect candidate HTML paths and pick a slice.
    all_paths = run_7z_list(args.archive)
    html_re = re.compile(rf"^OJ题库/{re.escape(args.source)}/(\d+)\.html$")
    candidates: List[Tuple[int, str]] = []
    for p in all_paths:
        m = html_re.match(p)
        if not m:
            continue
        try:
            pid = int(m.group(1))
        except ValueError:
            continue
        if pid >= args.start:
            candidates.append((pid, p))

    candidates.sort(key=lambda x: x[0])
    selected = candidates[: args.count]

    if not selected:
        print("no html candidates selected (check --source/--start)", file=sys.stderr)
        return 3

    out_lines: List[str] = []
    created = 0
    skipped = 0

    qdir = args.questions_dir
    for pid, inner in selected:
        fallback = str(pid)
        try:
            html = run_7z_cat(args.archive, inner)
        except subprocess.CalledProcessError as e:
            print(f"failed to read {inner}: {e}", file=sys.stderr)
            continue

        p = parse_problem(html, fallback)
        if p.time_limit_s < 1:
            p.time_limit_s = 1
        if args.max_cpu > 0 and p.time_limit_s > args.max_cpu:
            p.time_limit_s = args.max_cpu
        title_list = sanitize_title_for_list(p.title)

        # Ensure sample exists; otherwise the harness will always WA unless user prints nothing.
        # Still import, but warn.
        if not p.sample_out.strip():
            pass

        out_dir = qdir / p.number
        if out_dir.exists() and not args.overwrite:
            skipped += 1
        else:
            if args.write:
                write_problem(out_dir, p, overwrite=args.overwrite)
                created += 1

        out_lines.append(f"{p.number} {title_list} {p.star} {p.time_limit_s} {p.mem_limit_kb}")

    if args.print_list:
        print("\n".join(out_lines))

    print(f"selected={len(selected)} created={created} skipped={skipped}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
