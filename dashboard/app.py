from pathlib import Path
import json
import pandas as pd
import plotly.express as px
import streamlit as st

st.set_page_config(
    page_title="ToyOS Kernel Analytics",
    page_icon="⚙️",
    layout="wide"
)

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"

LOGS = {
    "Disabled - Initial": ("pi-disabled-20261112.jsonl", "Disabled"),
    "Disabled - Repeat 1": ("pi-disabled-repeat1-20261112.jsonl", "Disabled"),
    "Disabled - Repeat 2": ("pi-disabled-repeat2-20261112.jsonl", "Disabled"),
    "Enabled - Initial": ("pi-enabled-20261112.jsonl", "Enabled"),
    "Enabled - Repeat 1": ("pi-enabled-repeat1-20261112.jsonl", "Enabled"),
    "Enabled - Repeat 2": ("pi-enabled-repeat2-20261112.jsonl", "Enabled"),
}


def filesystem_evidence():
    """Verify filesystem persistence artifacts from the November 12 QEMU test."""
    image_path = BUILD / "filesystem-runtime-test.img"
    log_path = BUILD / "filesystem-runtime-test-fourth-boot.jsonl"

    expected_name = "runtime-write-check"
    expected_content = "toyos persistent write verification 2026\nabcd\n"

    disk_name = ""
    disk_content = ""
    disk_verified = False
    boot_verified = False

    image_available = image_path.exists()
    log_available = log_path.exists()

    if image_available:
        try:
            raw = image_path.read_bytes()
            slot_offset = 1024 + (2 * 2080)

            if len(raw) >= slot_offset + 2080:
                disk_name = raw[
                    slot_offset:slot_offset + 80
                ].split(b"\0", 1)[0].decode("ascii", errors="replace")

                disk_content = raw[
                    slot_offset + 80:slot_offset + 2080
                ].split(b"\0", 1)[0].decode("ascii", errors="replace")

                disk_verified = (
                    disk_name == expected_name
                    and disk_content == expected_content
                )
        except OSError:
            pass

    if log_available:
        try:
            with log_path.open(encoding="utf-8", errors="replace") as log_file:
                for line in log_file:
                    try:
                        record = json.loads(line)
                        if (
                            record.get("event") == "kernel.log"
                            and record.get("message") == "FILESYSTEM LOADED FROM DISK"
                            and record.get("tick") == 0
                        ):
                            boot_verified = True
                            break
                    except (json.JSONDecodeError, AttributeError):
                        continue
        except OSError:
            pass

    return {
        "image_available": image_available,
        "log_available": log_available,
        "disk_verified": disk_verified,
        "boot_verified": boot_verified,
        "disk_name": disk_name,
        "disk_content": disk_content,
        "image_size": image_path.stat().st_size if image_available else 0,
        "log_size": log_path.stat().st_size if log_available else 0,
    }


@st.cache_data
def load_data():
    records = []
    status = []

    for run, (filename, config) in LOGS.items():
        path = BUILD / filename
        valid = 0
        invalid = 0

        if not path.exists():
            status.append({
                "Run": run,
                "Status": "Missing",
                "Valid records": 0,
                "Invalid lines": 0
            })
            continue

        with path.open(encoding="utf-8", errors="replace") as f:
            for line in f:
                try:
                    item = json.loads(line)
                    item["run"] = run
                    item["configuration"] = config
                    records.append(item)
                    valid += 1
                except (json.JSONDecodeError, TypeError):
                    invalid += 1

        status.append({
            "Run": run,
            "Status": "Loaded",
            "Valid records": valid,
            "Invalid lines": invalid
        })

    return pd.DataFrame(records), pd.DataFrame(status)


df, file_status = load_data()

if df.empty:
    st.error("No valid benchmark records found.")
    st.stop()

for column in ["tick", "uptime_ms"]:
    df[column] = pd.to_numeric(df[column], errors="coerce")


def event_time(group, message):
    found = group[
        (group["event"] == "kernel.log") &
        (group["message"] == message)
    ]
    if found.empty:
        return None
    return float(found.iloc[0]["uptime_ms"])


def calculate_results():
    results = []

    for run, (_, config) in LOGS.items():
        group = df[df["run"] == run]
        if group.empty:
            continue

        low_start = event_time(group, "LOW HOLDS MUTEX")
        high_start = event_time(group, "HIGH TRYING MUTEX")
        low_end = event_time(group, "LOW RELEASING MUTEX")
        high_end = event_time(group, "HIGH ACQUIRED MUTEX")
        medium_end = event_time(group, "MEDIUM FINISHED")

        inherit_count = len(group[
            group["message"] == "PRIORITY INHERIT"
        ])

        samples = group[group["event"] == "performance.sample"]

        results.append({
            "Run": run,
            "Configuration": config,
            "High Mutex Wait (ms)": (
                high_end - high_start
                if high_end is not None and high_start is not None
                else None
            ),
            "Low Mutex Hold (ms)": (
                low_end - low_start
                if low_end is not None and low_start is not None
                else None
            ),
            "Medium Finish (ms)": medium_end,
            "PI Events": inherit_count,
            "Performance Samples": len(samples),
            "Last Sample Tick": (
                samples["tick"].max() if not samples.empty else None
            )
        })

    return pd.DataFrame(results)


results = calculate_results()

st.markdown("""
<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap');

:root {
    --bg: #0a0f1a;
    --surface: #111a2b;
    --surface-2: #16223a;
    --border: #243350;
    --text: #f3f6fb;
    --text-2: #c3cfe2;
    --muted: #93a4bf;
    --blue: #3b9eff;
    --teal: #2dd4bf;
    --amber: #fbbf24;
    --violet: #a78bfa;
    --radius: 12px;
}

/* Base */
html, body, [class*="css"] {
    font-family: 'Inter', -apple-system, 'Segoe UI', sans-serif;
    font-feature-settings: "tnum" 1, "cv11" 1;
}

.stApp {
    background: var(--bg);
    color: var(--text);
}

[data-testid="stHeader"] {
    background: rgba(10, 15, 26, 0.9);
    border-bottom: 1px solid var(--border);
}

#MainMenu, footer { visibility: hidden; }

.block-container {
    padding: 2rem 3rem 4rem 3rem;
    max-width: 1600px;
}

/* Typography */
h1 {
    color: var(--text) !important;
    font-size: 2.2rem !important;
    font-weight: 800 !important;
    letter-spacing: -0.03em !important;
    line-height: 1.2 !important;
    padding-bottom: 0.25rem !important;
}

h2 {
    color: var(--text) !important;
    font-size: 1.5rem !important;
    font-weight: 700 !important;
    letter-spacing: -0.02em !important;
    margin-top: 2rem !important;
    padding-bottom: 0.6rem !important;
    border-bottom: 1px solid var(--border);
}

h3 {
    color: var(--text-2) !important;
    font-size: 1.1rem !important;
    font-weight: 600 !important;
    margin-top: 1.25rem !important;
}

p, li, label {
    color: var(--text-2);
    font-size: 1rem;
    line-height: 1.6;
}

[data-testid="stCaptionContainer"] p {
    color: var(--muted) !important;
    font-size: 0.875rem !important;
}

hr {
    border-color: var(--border) !important;
    margin: 1.75rem 0 !important;
}

/* Sidebar */
[data-testid="stSidebar"] {
    background: #0d1524;
    border-right: 1px solid var(--border);
}

[data-testid="stSidebar"] > div:first-child {
    padding-top: 1.5rem;
}

[data-testid="stSidebar"] h1 {
    font-size: 1.5rem !important;
    letter-spacing: -0.02em !important;
}

[data-testid="stSidebar"] [data-testid="stMarkdownContainer"] p {
    color: var(--muted);
    font-size: 0.9rem;
}

[data-testid="stSidebar"] [data-testid="stRadio"] label {
    padding: 0.55rem 0.8rem;
    border-radius: 8px;
    width: 100%;
    font-size: 1rem !important;
    font-weight: 500 !important;
    color: var(--text-2);
    transition: background 0.15s ease;
}

[data-testid="stSidebar"] [data-testid="stRadio"] label:hover {
    background: var(--surface-2);
}

[data-testid="stSidebar"] [data-testid="stRadio"] label:has(input:checked) {
    background: var(--surface-2);
    box-shadow: inset 3px 0 0 var(--blue);
    color: var(--text);
    font-weight: 600 !important;
}

/* KPI cards */
[data-testid="stMetric"] {
    background: var(--surface);
    border: 1px solid var(--border);
    border-left: 4px solid var(--blue);
    border-radius: var(--radius);
    padding: 1.4rem 1.6rem;
    min-height: 150px;
    display: flex;
    flex-direction: column;
    justify-content: center;
    gap: 0.35rem;
}

[data-testid="stHorizontalBlock"] > div:nth-child(4n+1) [data-testid="stMetric"] { border-left-color: var(--blue); }
[data-testid="stHorizontalBlock"] > div:nth-child(4n+2) [data-testid="stMetric"] { border-left-color: var(--teal); }
[data-testid="stHorizontalBlock"] > div:nth-child(4n+3) [data-testid="stMetric"] { border-left-color: var(--amber); }
[data-testid="stHorizontalBlock"] > div:nth-child(4n+4) [data-testid="stMetric"] { border-left-color: var(--violet); }

[data-testid="stMetricLabel"],
[data-testid="stMetricLabel"] p {
    color: var(--muted) !important;
    font-size: 0.95rem !important;
    font-weight: 600 !important;
}

[data-testid="stMetricValue"],
[data-testid="stMetricValue"] div {
    color: var(--text) !important;
    font-size: 2.6rem !important;
    font-weight: 800 !important;
    letter-spacing: -0.03em;
    line-height: 1.1 !important;
}

[data-testid="stMetricDelta"] {
    font-size: 0.9rem !important;
    font-weight: 600 !important;
}

/* Layout spacing */
[data-testid="stHorizontalBlock"] {
    gap: 1.25rem;
}

[data-testid="stVerticalBlock"] {
    gap: 1rem;
}

/* Charts and tables */
[data-testid="stPlotlyChart"] {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 1rem 0.75rem 0.5rem 0.75rem;
}

div[data-testid="stDataFrame"] {
    border: 1px solid var(--border);
    border-radius: var(--radius);
    overflow: hidden;
}

/* Tabs */
[data-baseweb="tab-list"] {
    gap: 0.5rem;
    border-bottom: 1px solid var(--border);
}

[data-baseweb="tab"] {
    font-size: 1rem;
    font-weight: 600;
    color: var(--muted);
    padding: 0.7rem 1.1rem;
}

[data-baseweb="tab"][aria-selected="true"] {
    color: var(--text);
}

[data-baseweb="tab-highlight"] {
    background-color: var(--blue) !important;
    height: 3px;
}

/* Expanders */
[data-testid="stExpander"] {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
}

/* Inputs and buttons */
.stButton button, .stDownloadButton button {
    background: var(--blue);
    color: #04101f;
    border: 0;
    border-radius: 8px;
    font-weight: 700;
    font-size: 0.95rem;
    padding: 0.6rem 1.25rem;
    transition: background 0.15s ease;
}

.stButton button:hover, .stDownloadButton button:hover {
    background: #66b3ff;
    color: #04101f;
    border: 0;
}

[data-baseweb="select"] > div,
[data-baseweb="input"] > div,
[data-baseweb="textarea"] > div {
    background-color: var(--surface);
    border-color: var(--border);
    border-radius: 8px;
    font-size: 0.95rem;
}

[data-baseweb="select"] > div:focus-within,
[data-baseweb="input"] > div:focus-within {
    border-color: var(--blue);
}

.stAlert {
    border-radius: var(--radius);
}

/* Mobile */
@media (max-width: 768px) {
    .block-container { padding: 1.25rem 1rem 3rem 1rem; }
    h1 { font-size: 1.7rem !important; }
    [data-testid="stMetricValue"],
    [data-testid="stMetricValue"] div { font-size: 2rem !important; }
    [data-testid="stMetric"] { min-height: 120px; }
}
</style>
""", unsafe_allow_html=True)

st.sidebar.title("⚙️ ToyOS")
st.sidebar.caption("Kernel Performance Analytics")

page = st.sidebar.radio(
    "Navigation",
    ["Overview", "Priority Inheritance", "Scheduler & Memory", "Raw Kernel Logs"]
)

st.title("ToyOS Kernel Analytics")
st.caption("Operating System Performance Monitoring Dashboard")
st.divider()

# --- TECH THEME HELPERS START ---
import html as _html
import plotly.graph_objects as go

TECH = {
    "bg": "#0a0f1a",
    "panel": "#111a2b",
    "grid": "#1c2a44",
    "border": "#243350",
    "text": "#e6edf7",
    "muted": "#8a9bb4",
    "cyan": "#22d3ee",
    "steel": "#64748b",
    "amber": "#fbbf24",
    "green": "#34d399",
    "red": "#f87171",
}

st.markdown("""
<style>
@import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;500;600;700&family=IBM+Plex+Mono:wght@500;600&display=swap');

html, body, [class*="css"] { font-family: 'IBM Plex Sans', 'Inter', sans-serif; }

.stApp { background: #0a0f1a; color: #e6edf7; }
.block-container { max-width: 1600px; padding: 2rem 3rem 4rem 3rem; }
#MainMenu, footer { visibility: hidden; }

/* Hero banner */
.hero {
    background: #0e1626;
    border: 1px solid #243350;
    border-left: 5px solid #22d3ee;
    border-radius: 12px;
    padding: 2rem 2.2rem;
    margin-bottom: 2.2rem;
}
.hero-title {
    font-size: 3rem; font-weight: 700; color: #f8fbff;
    letter-spacing: -0.03em; line-height: 1.1;
}
.hero-sub {
    font-size: 1.2rem; color: #8fb4d9; margin-top: 0.6rem; font-weight: 500;
}
.chips { margin-top: 1.2rem; display: flex; flex-wrap: wrap; gap: 0.6rem; }
.chip {
    font-family: 'IBM Plex Mono', monospace; font-size: 0.85rem; font-weight: 500;
    color: #c3cfe2; background: #16223a; border: 1px solid #243350;
    border-radius: 6px; padding: 0.3rem 0.7rem;
}

/* Section headings */
.sec-title {
    font-size: 2rem; font-weight: 700; color: #f8fbff;
    letter-spacing: -0.02em; margin: 2.6rem 0 0.25rem 0;
    padding-left: 0.8rem; border-left: 4px solid #22d3ee; line-height: 1.2;
}
.sec-sub {
    font-size: 1.1rem; color: #8fb4d9; margin: 0 0 1.2rem 1.05rem; font-weight: 400;
}

/* KPI cards */
[data-testid="stMetric"] {
    background: #111a2b;
    border: 1px solid #243350;
    border-top: 3px solid #22d3ee;
    border-radius: 10px;
    padding: 1.4rem 1.6rem;
    min-height: 150px;
    display: flex; flex-direction: column; justify-content: center; gap: 0.4rem;
}
[data-testid="stMetricLabel"], [data-testid="stMetricLabel"] p {
    color: #8fb4d9 !important; font-size: 1rem !important; font-weight: 600 !important;
}
[data-testid="stMetricValue"], [data-testid="stMetricValue"] div {
    font-family: 'IBM Plex Mono', monospace !important;
    color: #f8fbff !important; font-size: 2.6rem !important;
    font-weight: 600 !important; letter-spacing: -0.03em; line-height: 1.1 !important;
}

/* Layout spacing */
[data-testid="stHorizontalBlock"] { gap: 1.25rem; }

/* Charts, tables, containers */
[data-testid="stPlotlyChart"] {
    background: #111a2b; border: 1px solid #243350;
    border-radius: 10px; padding: 0.8rem 0.6rem 0.2rem 0.6rem;
}
div[data-testid="stDataFrame"] {
    border: 1px solid #243350; border-radius: 10px; overflow: hidden;
}
[data-testid="stVerticalBlockBorderWrapper"] {
    background: #111a2b; border-color: #243350 !important; border-radius: 10px;
}
[data-testid="stCaptionContainer"] p { color: #8a9bb4 !important; font-size: 0.9rem !important; }
p, li { color: #c3cfe2; font-size: 1.02rem; line-height: 1.65; }
.stAlert { border-radius: 10px; }

/* Log file cards */
.log-card {
    background: #111a2b; border: 1px solid #243350; border-radius: 10px;
    padding: 1.2rem 1.4rem; margin-bottom: 1.25rem;
}
.log-top { display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem; }
.log-run { font-size: 1.15rem; font-weight: 600; color: #f8fbff; }
.pill {
    font-family: 'IBM Plex Mono', monospace; font-size: 0.8rem; font-weight: 600;
    border-radius: 999px; padding: 0.2rem 0.75rem; border: 1px solid;
}
.pill.ok  { color: #34d399; border-color: #1f6b52; background: rgba(52, 211, 153, 0.08); }
.pill.bad { color: #f87171; border-color: #7a2f2f; background: rgba(248, 113, 113, 0.08); }
.log-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; }
.log-val { font-family: 'IBM Plex Mono', monospace; font-size: 1.7rem; font-weight: 600; color: #f8fbff; }
.log-lab { font-size: 0.9rem; color: #8a9bb4; margin-top: 0.15rem; }
.log-note { font-size: 0.85rem; color: #8a9bb4; margin-top: 1rem; border-top: 1px solid #243350; padding-top: 0.7rem; }

@media (max-width: 768px) {
    .block-container { padding: 1.2rem 1rem 3rem 1rem; }
    .hero-title { font-size: 2rem; }
    .sec-title { font-size: 1.5rem; }
    [data-testid="stMetricValue"], [data-testid="stMetricValue"] div { font-size: 2rem !important; }
}
</style>
""", unsafe_allow_html=True)


def section(title, subtitle=""):
    """Large section heading with a one-line description."""
    st.markdown(
        f'<div class="sec-title">{_html.escape(title)}</div>'
        f'<div class="sec-sub">{_html.escape(subtitle)}</div>',
        unsafe_allow_html=True,
    )


def tech_fig(fig, height=380):
    """Apply the dark engineering theme to any Plotly figure.
    Use on every page:  st.plotly_chart(tech_fig(fig), width="stretch")"""
    fig.update_layout(
        template="plotly_dark",
        paper_bgcolor="rgba(0,0,0,0)",
        plot_bgcolor="rgba(0,0,0,0)",
        height=height,
        margin=dict(l=20, r=20, t=60, b=20),
        font=dict(family="IBM Plex Sans, Inter, sans-serif", color="#c3cfe2", size=14),
        title=dict(font=dict(size=20, color="#f8fbff"), x=0.01),
        colorway=[TECH["cyan"], TECH["steel"], TECH["amber"], TECH["green"], "#a78bfa", TECH["red"]],
        legend=dict(orientation="h", y=1.12, x=1, xanchor="right",
                    font=dict(size=13), bgcolor="rgba(0,0,0,0)"),
        hoverlabel=dict(bgcolor="#16223a", bordercolor="#243350",
                        font=dict(family="IBM Plex Mono, monospace", size=13)),
    )
    fig.update_xaxes(gridcolor=TECH["grid"], linecolor=TECH["border"],
                     zerolinecolor=TECH["border"], tickfont=dict(size=13))
    fig.update_yaxes(gridcolor=TECH["grid"], linecolor=TECH["border"],
                     zerolinecolor=TECH["border"], tickfont=dict(size=13))
    return fig
# --- TECH THEME HELPERS END ---

if page == "Overview":
    # ---------- Data preparation ----------
    performance = df[df["event"] == "performance.sample"]
    inherit_count = int((df["message"] == "PRIORITY INHERIT").sum())

    loaded_count = int((file_status["Status"] == "Loaded").sum())
    missing_count = int((file_status["Status"] == "Missing").sum())
    invalid_total = int(file_status["Invalid lines"].sum())
    valid_total = int(file_status["Valid records"].sum())

    summary = results.copy()
    for col in ["High Mutex Wait (ms)", "Low Mutex Hold (ms)", "Medium Finish (ms)"]:
        summary[col] = pd.to_numeric(summary[col], errors="coerce")

    disabled = summary[summary["Configuration"] == "Disabled"]
    enabled = summary[summary["Configuration"] == "Enabled"]

    off_avg = disabled["High Mutex Wait (ms)"].mean()
    on_avg = enabled["High Mutex Wait (ms)"].mean()

    reduction = (
        (off_avg - on_avg) / off_avg * 100
        if pd.notna(off_avg) and pd.notna(on_avg) and off_avg != 0
        else None
    )

    # ---------- Header ----------
    st.markdown(
        '<div class="hero">'
        '<div class="hero-title">Kernel Benchmark Observability</div>'
        '<div class="hero-sub">Runtime telemetry and priority inheritance experiment</div>'
        '<div class="chips">'
        f'<span class="chip">{loaded_count}/6 logs loaded</span>'
        f'<span class="chip">{len(df):,} JSON records</span>'
        f'<span class="chip">{len(performance):,} performance samples</span>'
        f'<span class="chip">{inherit_count:,} PI inherit events</span>'
        '</div></div>',
        unsafe_allow_html=True,
    )

    # ---------- 1. Runtime snapshot ----------
    section("Runtime Data Snapshot", "Volume of telemetry captured across all benchmark runs")

    c1, c2, c3, c4 = st.columns(4)
    c1.metric("Benchmark Runs", f"{len(results)}",
              help="Runs represented in the loaded benchmark data")
    c2.metric("Performance Samples", f"{len(performance):,}")
    c3.metric("PI Inherit Events", f"{inherit_count:,}")
    c4.metric("Valid JSON Records", f"{len(df):,}")

    # ---------- 2. Key findings ----------
    section("Priority Inheritance: Key Findings",
            "Average HIGH-task mutex wait across available benchmark runs")

    a, b, c = st.columns(3)
    a.metric("PI Disabled", f"{off_avg:.2f} ms" if pd.notna(off_avg) else "N/A")
    b.metric("PI Enabled", f"{on_avg:.2f} ms" if pd.notna(on_avg) else "N/A")
    c.metric("Observed Wait Reduction",
             f"{reduction:.2f}%" if reduction is not None else "N/A")

    left, right = st.columns(2)

    with left:
        run_df = summary.dropna(subset=["High Mutex Wait (ms)"])
        if run_df.empty:
            st.info("No HIGH-task wait data available to chart.")
        else:
            fig_runs = go.Figure()
            for cfg, color in (("Disabled", TECH["steel"]), ("Enabled", TECH["cyan"])):
                part = run_df[run_df["Configuration"] == cfg]
                if not part.empty:
                    fig_runs.add_bar(
                        x=part["Run"], y=part["High Mutex Wait (ms)"],
                        name=f"PI {cfg}", marker_color=color,
                        texttemplate="%{y:.2f}", textposition="outside",
                    )
            fig_runs.update_layout(
                title="HIGH-task mutex wait per run",
                yaxis_title="Wait (ms)", barmode="overlay",
            )
            st.plotly_chart(tech_fig(fig_runs), width="stretch")

    with right:
        metric_cols = {
            "HIGH wait": "High Mutex Wait (ms)",
            "LOW hold": "Low Mutex Hold (ms)",
            "MEDIUM finish": "Medium Finish (ms)",
        }
        fig_avg = go.Figure()
        has_data = False
        for cfg, color in (("Disabled", TECH["steel"]), ("Enabled", TECH["cyan"])):
            part = summary[summary["Configuration"] == cfg]
            if part.empty:
                continue
            values = [part[col].mean() for col in metric_cols.values()]
            if all(pd.isna(v) for v in values):
                continue
            has_data = True
            fig_avg.add_bar(
                x=list(metric_cols.keys()), y=values,
                name=f"PI {cfg}", marker_color=color,
                texttemplate="%{y:.2f}", textposition="outside",
            )
        if has_data:
            fig_avg.update_layout(
                title="Average task timing by configuration",
                yaxis_title="Time (ms)", barmode="group",
            )
            st.plotly_chart(tech_fig(fig_avg), width="stretch")
        else:
            st.info("No timing data available to chart.")

    with st.container(border=True):
        st.markdown("**Experimental interpretation**")
        if reduction is not None:
            st.write(
                f"In this workload, enabling priority inheritance was "
                f"associated with a {reduction:.2f}% reduction in average "
                f"HIGH-task mutex wait time."
            )
        else:
            st.write("Insufficient benchmark data to calculate the wait reduction.")
        st.caption(
            "This is an observed result for the current benchmark workload, "
            "not a universal performance guarantee."
        )

    # ---------- 3. Benchmark summary table ----------
    section("Benchmark Summary",
            "Per-run measurements extracted from the six kernel JSONL log files")

    st.dataframe(
        summary,
        width="stretch",
        hide_index=True,
        column_config={
            "Run": st.column_config.TextColumn("Benchmark Run"),
            "Configuration": st.column_config.TextColumn("PI Configuration"),
            "High Mutex Wait (ms)": st.column_config.NumberColumn("HIGH Wait", format="%.2f ms"),
            "Low Mutex Hold (ms)": st.column_config.NumberColumn("LOW Hold", format="%.2f ms"),
            "Medium Finish (ms)": st.column_config.NumberColumn("MEDIUM Finish", format="%.2f ms"),
            "PI Events": st.column_config.NumberColumn("PI Events", format="%d"),
            "Performance Samples": st.column_config.NumberColumn("Samples", format="%d"),
            "Last Sample Tick": st.column_config.NumberColumn("Last Tick", format="%d"),
        },
    )

    # ---------- 4. Log file health ----------
    section("Log File Health", "Input validation and availability for every benchmark run")

    h1, h2, h3, h4 = st.columns(4)
    h1.metric("Files Loaded", f"{loaded_count}/6")
    h2.metric("Missing Files", f"{missing_count}")
    h3.metric("Valid Records", f"{valid_total:,}")
    h4.metric("Non-JSON Lines", f"{invalid_total:,}")

    st.write("")

    for row_start in range(0, len(file_status), 3):
        card_columns = st.columns(3)
        for offset, (_, file_row) in enumerate(
            file_status.iloc[row_start:row_start + 3].iterrows()
        ):
            status = str(file_row["Status"])
            pill = "ok" if status == "Loaded" else "bad"
            valid = int(file_row["Valid records"])
            invalid = int(file_row["Invalid lines"])
            note = (
                "Non-JSON input lines were skipped by the parser."
                if invalid > 0 else "No invalid lines detected."
            )
            with card_columns[offset]:
                st.markdown(
                    '<div class="log-card">'
                    '<div class="log-top">'
                    f'<span class="log-run">{_html.escape(str(file_row["Run"]))}</span>'
                    f'<span class="pill {pill}">{_html.escape(status.upper())}</span>'
                    '</div>'
                    '<div class="log-grid">'
                    f'<div><div class="log-val">{valid:,}</div><div class="log-lab">Valid records</div></div>'
                    f'<div><div class="log-val">{invalid:,}</div><div class="log-lab">Invalid lines</div></div>'
                    '</div>'
                    f'<div class="log-note">{note}</div>'
                    '</div>',
                    unsafe_allow_html=True,
                )

    if missing_count > 0:
        st.warning(
            "One or more benchmark log files are missing. "
            "Results may not represent all six runs."
        )
    else:
        st.success("All six expected benchmark log files are available.")

    if invalid_total > 0:
        st.info(
            "Invalid lines are excluded from structured analytics. "
            "The plain-text Ring 3 output is not a JSON telemetry record."
        )

    # ---------- Filesystem runtime persistence evidence ----------
    st.markdown("---")
    section(
        "Filesystem Runtime Persistence",
        "Independent artifact verification · Interactive QEMU test · 12 November 2026"
    )

    fs = filesystem_evidence()

    fs_col1, fs_col2, fs_col3 = st.columns(3)

    with fs_col1:
        st.metric(
            "Disk Image Content",
            "VERIFIED" if fs["disk_verified"] else "NOT VERIFIED"
        )

    with fs_col2:
        st.metric(
            "Fourth-Boot Load",
            "CONFIRMED" if fs["boot_verified"] else "NOT CONFIRMED"
        )

    with fs_col3:
        st.metric(
            "Test Image",
            "AVAILABLE" if fs["image_available"] else "MISSING"
        )

    if fs["disk_verified"] and fs["boot_verified"]:
        st.success(
            "Both independent artifact checks passed: the disk image "
            "contains the expected filename and saved text, and the "
            "fourth-boot log confirms filesystem loading."
        )
    elif not fs["image_available"] or not fs["log_available"]:
        st.info(
            "One or more evidence artifacts are unavailable. "
            "Run the filesystem persistence test to populate this panel."
        )
    else:
        st.warning(
            "One or more artifact checks did not match the expected evidence. "
            "Inspect the disk image and fourth-boot log."
        )

    with st.expander("Runtime Test Procedure and Evidence Details"):
        st.markdown(
            '''
            **Verified operation sequence**

            1. Boot ToyOS with the isolated IDE disk image.
            2. List files and read existing stored content.
            3. Create `runtime-write-check`.
            4. Edit and save the file.
            5. Inspect the disk image for the saved filename and content.
            6. Reboot using the same disk image.
            7. Read the saved file again in the interactive QEMU shell.

            **Expected saved content**

            ```text
            toyos persistent write verification 2026
            abcd
            ```

            **Evidence interpretation**

            - Disk-image inspection independently checks the filename and exact saved content.
            - The fourth-boot serial log checks the `FILESYSTEM LOADED FROM DISK` event at tick 0.
            - The post-reboot interactive shell read-back was manually observed. It is not represented as a structured serial event.
            '''
        )

        st.caption(
            f"Disk image: {fs['image_size']:,} bytes · "
            f"Fourth-boot log: {fs['log_size']:,} bytes"
        )

elif page == "Priority Inheritance":
    st.header("Priority Inheritance Evaluation")

    disabled = results[results["Configuration"] == "Disabled"]
    enabled = results[results["Configuration"] == "Enabled"]

    off_wait = disabled["High Mutex Wait (ms)"].mean()
    on_wait = enabled["High Mutex Wait (ms)"].mean()

    off_hold = disabled["Low Mutex Hold (ms)"].mean()
    on_hold = enabled["Low Mutex Hold (ms)"].mean()

    c1, c2, c3, c4 = st.columns(4)
    c1.metric("Wait - PI Off", f"{off_wait:.2f} ms")
    c2.metric("Wait - PI On", f"{on_wait:.2f} ms")
    c3.metric("Wait Reduction", f"{(off_wait-on_wait)/off_wait*100:.2f}%")
    c4.metric("Hold Reduction", f"{(off_hold-on_hold)/off_hold*100:.2f}%")

    st.subheader("HIGH Task Mutex Wait")
    fig = px.bar(
        results,
        x="Run",
        y="High Mutex Wait (ms)",
        color="Configuration",
        barmode="group",
        text="High Mutex Wait (ms)",
        template="plotly_dark",
        color_discrete_map={
            "Disabled": "#ef8354",
            "Enabled": "#2a9d8f"
        }
    )
    st.plotly_chart(fig, use_container_width=True)

    st.subheader("LOW Task Mutex Hold")
    fig2 = px.bar(
        results,
        x="Run",
        y="Low Mutex Hold (ms)",
        color="Configuration",
        text="Low Mutex Hold (ms)",
        template="plotly_dark"
    )
    st.plotly_chart(fig2, use_container_width=True)

    st.subheader("Detailed Results")
    st.dataframe(results, use_container_width=True, hide_index=True)

    st.warning(
        "The initial PI-disabled run has a longer telemetry window "
        "(8740 ticks versus 160-170 ticks in the other runs). "
        "Compare event durations separately from cumulative telemetry."
    )

elif page == "Scheduler & Memory":
    st.header("Scheduler and Memory Analytics")

    perf = df[df["event"] == "performance.sample"].copy()

    for column in [
        "task_switches_total",
        "memory_used_pages",
        "memory_total_pages"
    ]:
        perf[column] = pd.to_numeric(perf[column], errors="coerce")

    perf["Memory Usage (%)"] = (
        perf["memory_used_pages"] /
        perf["memory_total_pages"].replace(0, pd.NA) * 100
    )

    selected = st.multiselect(
        "Select runs",
        list(LOGS.keys()),
        default=list(LOGS.keys())
    )
    view = perf[perf["run"].isin(selected)]

    st.subheader("Cumulative Task Switches")
    fig = px.line(
        view,
        x="uptime_ms",
        y="task_switches_total",
        color="run",
        template="plotly_dark"
    )
    st.plotly_chart(fig, use_container_width=True)

    st.subheader("Physical Memory Usage")
    fig2 = px.line(
        view,
        x="uptime_ms",
        y="Memory Usage (%)",
        color="run",
        template="plotly_dark"
    )
    st.plotly_chart(fig2, use_container_width=True)

    st.subheader("Scheduler Event Counts")
    scheduler = df[
        df["message"].isin([
            "TASK SWITCH",
            "MLFQ QUEUE DEMOTION",
            "MLFQ AGING BOOST",
            "CURRENT TASK BLOCKED",
            "TASK WOKEN"
        ])
    ]

    if scheduler.empty:
        st.info("No matching scheduler events found.")
    else:
        counts = scheduler.groupby(
            ["run", "message"]
        ).size().reset_index(name="Count")

        fig3 = px.bar(
            counts,
            x="message",
            y="Count",
            color="run",
            barmode="group",
            template="plotly_dark"
        )
        st.plotly_chart(fig3, use_container_width=True)

    st.caption(
        "Queue-level history is not inferred because the current "
        "performance samples do not record queue levels."
    )

elif page == "Raw Kernel Logs":
    st.header("Raw Kernel Log Explorer")

    run = st.selectbox("Select run", ["All runs"] + list(LOGS.keys()))
    event = st.selectbox(
        "Select event",
        ["All events"] + sorted(df["event"].dropna().unique().tolist())
    )
    search = st.text_input("Search log messages")

    view = df.copy()

    if run != "All runs":
        view = view[view["run"] == run]

    if event != "All events":
        view = view[view["event"] == event]

    if search:
        view = view[
            view.astype(str).apply(
                lambda col: col.str.contains(
                    search, case=False, na=False
                )
            ).any(axis=1)
        ]

    st.write(f"Records found: {len(view):,}")
    st.dataframe(view, use_container_width=True, hide_index=True)

    st.download_button(
        "Download filtered logs as CSV",
        view.to_csv(index=False).encode("utf-8"),
        "toyos_filtered_logs.csv",
        "text/csv"
    )

st.divider()
st.caption("ToyOS | Experimental Kernel Analytics | Data from JSONL logs")
