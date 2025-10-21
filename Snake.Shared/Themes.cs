// Snake.Shared/Themes.cs
using System.Collections.Generic;
using System.Linq;

namespace Snake.Shared
{
    public static class ThemeCatalog
    {
        public sealed record ThemeItem(
            string Id, string Display, int Price, int MinLevel,
            string BgHex, string GridHex, string TextHex, string AppleHex,
            string GoldHex, string XpHex);

        public static readonly List<ThemeItem> AllThemes = new()
        {
            // ── 기본 ──
            new("theme_dark",    "다크",        0,   1, "#111111", "#1f1f1f", "#ffffff", "#ff5555", "#ffd700", "#b388ff"),
            new("theme_retro",   "레트로",    120,   3, "#001b2e", "#073b4c", "#d8f3dc", "#ff9e00", "#ffd700", "#a78bfa"),
            new("theme_neon",    "네온",      200,   5, "#0b0f1a", "#192734", "#66fcf1", "#ff4df5", "#ffd700", "#b388ff"),
            new("theme_candy",   "캔디",      280,   7, "#2b193d", "#3e1f47", "#ffe3e3", "#ff6f91", "#ffd700", "#b388ff"),
            new("theme_mint",    "민트",      220,   6, "#003b36", "#0b6e4f", "#d2f1e4", "#4cd3c2", "#ffd700", "#a78bfa"),
            new("theme_ocean",   "오션",      240,   6, "#002b36", "#073642", "#93a1a1", "#2aa198", "#ffd700", "#7c4dff"),
            new("theme_forest",  "포레스트",  260,   7, "#0f2d2e", "#1b4332", "#d8f3dc", "#52b788", "#ffd700", "#b388ff"),
            new("theme_space",   "스페이스",  320,   9, "#0a0a23", "#1c1c3c", "#b3b3ff", "#ff5e5b", "#ffd700", "#b388ff"),
            new("theme_sunset",  "선셋",      300,   8, "#261a1a", "#4b2e2e", "#ffe8d6", "#ff7f50", "#ffd700", "#a78bfa"),

            // ── 확장 (눈에 띄는 컨셉 위주) ──
            new("theme_pastel",     "파스텔",        200,  5, "#23252b", "#2f3138", "#f8fafc", "#f0abfc", "#ffd700", "#c4b5fd"),
            new("theme_midnight",   "미드나잇",      220,  6, "#0c1222", "#19233a", "#e2e8f0", "#38bdf8", "#ffd700", "#a78bfa"),
            new("theme_ocean_deep", "딥 오션",       240,  6, "#011627", "#0b2a3a", "#a0aec0", "#00c2d1", "#ffd700", "#7c4dff"),
            new("theme_sakura",     "사쿠라",        260,  7, "#2a2230", "#3a2b40", "#ffe4ef", "#ff85b3", "#ffd700", "#ffb4e6"),
            new("theme_honey",      "허니",          260,  7, "#20160c", "#3a2711", "#fff7e6", "#ffb703", "#ffd700", "#f0c6ff"),
            new("theme_glacier",    "글레이셔",      280,  8, "#0b132b", "#1c2541", "#e0f2fe", "#7dd3fc", "#ffd700", "#bde0fe"),
            new("theme_lava",       "라바",          280,  8, "#1a0b0b", "#2b1515", "#ffe4e6", "#ef4444", "#ffd700", "#f472b6"),
            new("theme_thunder",    "썬더",          300,  9, "#0b0b12", "#16162a", "#f8fafc", "#fde047", "#ffd700", "#c7d2fe"),
            new("theme_vaporwave",  "베이퍼웨이브",  300,  9, "#1a1033", "#2a174d", "#e7e5e4", "#ff77e9", "#ffd700", "#7dd3fc"),
            new("theme_space_nova", "스페이스 노바", 320, 10, "#0a0a1a", "#1a1a33", "#d6d3d1", "#93c5fd", "#ffd700", "#c4b5fd"),
            new("theme_phantom",    "팬텀",          320, 10, "#0b0d10", "#15181f", "#e5e7eb", "#94a3b8", "#ffd700", "#a78bfa"),
            new("theme_royal",      "로얄",          340, 11, "#0e1022", "#1b1f3a", "#e2e8f0", "#8b5cf6", "#ffd700", "#c4b5fd"),
            new("theme_jungle",     "정글",          340, 11, "#081c15", "#0f3d2e", "#d8f3dc", "#2dc653", "#ffd700", "#b7e4c7"),
            new("theme_crystal",    "크리스탈",      360, 12, "#0f1026", "#1b1d3a", "#e0e7ff", "#a78bfa", "#ffd700", "#d1c4ff"),
            new("theme_ember",      "엠버",          360, 12, "#1c0a0a", "#2b1111", "#ffe4e6", "#fb7185", "#ffd700", "#f0abfc"),
            new("theme_borealis",   "오로라",        380, 13, "#071521", "#0f2230", "#e5fff9", "#22d3ee", "#ffd700", "#93c5fd"),
            new("theme_starry",     "스타리",        380, 13, "#0a1025", "#121a3a", "#c7d2fe", "#60a5fa", "#ffd700", "#a5b4fc"),
            new("theme_cyberpunk",  "사이버펑크",    400, 14, "#0b0b16", "#17172b", "#e5e7eb", "#06b6d4", "#ffd700", "#f472b6"),
            new("theme_prism",      "프리즘",        420, 15, "#0e0f1a", "#1a1c2e", "#f8fafc", "#22c55e", "#ffd700", "#a78bfa"),
            new("theme_void",       "보이드",        420, 15, "#080812", "#121224", "#e2e8f0", "#38bdf8", "#ffd700", "#94a3b8"),
        };

        public static ThemeItem ThemeOf(string id)
            => AllThemes.FirstOrDefault(x => x.Id == id) ?? AllThemes[0];
    }
}
