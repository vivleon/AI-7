// Snake.Shared/Cosmetics.cs
using System.Linq;

namespace Snake.Shared;

public static class CosmeticCatalog
{
    public record SkinMeta(string Id, string Display, int Price, int MinLevel);

    // 명시적 배열 타입으로 변경 (암시적 배열 형식 추론 오류 방지)
    public static readonly SkinMeta[] AllSkins = new SkinMeta[]
    {
        // 심플(무료/저레벨)
        new SkinMeta("skin_basic_blue",   "Basic Blue",     0,  1),
        new SkinMeta("skin_basic_green",  "Basic Green",    0,  1),
        new SkinMeta("skin_basic_yellow","Basic Yellow",    0,  1),
        new SkinMeta("skin_basic_red",     "Basic Red",     0, 1),

        // 점차 업그레이드
        new SkinMeta("skin_stripe_red",   "Stripe Red",    20,  3),
        new SkinMeta("skin_dot_purple",   "Dot Purple",    30,  5),
        new SkinMeta("skin_neon_cyan",    "Neon Cyan",     35,  4),
        new SkinMeta("skin_neon_magenta", "Neon Magenta",  35,  4),
        new SkinMeta("skin_ice_crystal",  "Ice Crystal",   50,  6),
        new SkinMeta("skin_fire_flame",   "Fire Flame",    50,  6),
        new SkinMeta("skin_tiger",        "Tiger",         70,  8),
        new SkinMeta("skin_panther",      "Panther",       70,  8),
        new SkinMeta("skin_carbon",       "Carbon Fiber",  80,  9),
        new SkinMeta("skin_circuit",      "Circuit",       90, 10),
        new SkinMeta("skin_camo_forest",  "Camo Forest",    65,  7),
        new SkinMeta("skin_camo_desert",  "Camo Desert",    65,  7),

        // 화려/레어
        new SkinMeta("skin_arrow_gold",   "Arrow Gold",    60,  8),
        new SkinMeta("skin_royal_black",  "Royal Black",  100, 12),
        new SkinMeta("skin_rainbow",      "Rainbow",      120, 12),
        new SkinMeta("skin_dragon",       "Dragon",       150, 14),
        new SkinMeta("skin_phoenix",      "Phoenix",      150, 14),
        new SkinMeta("skin_void",         "Void",         180, 15),
        new SkinMeta("skin_aurora",       "Aurora",       180, 15),

        new SkinMeta("skin_stripe_teal",     "Stripe Teal",        20, 3),
        new SkinMeta("skin_dot_lime",        "Dot Lime",           30, 4),
        new SkinMeta("skin_honeycomb",       "Honeycomb",          45, 6),
        new SkinMeta("skin_metal_chrome",    "Metal Chrome",       60, 7),
        new SkinMeta("skin_obsidian",        "Obsidian",           70, 8),
        new SkinMeta("skin_glacier",         "Glacier",            80, 9),
        new SkinMeta("skin_lava",            "Lava",               90,10),
        new SkinMeta("skin_thunder",         "Thunder",           100,11),
        new SkinMeta("skin_shadow",          "Shadow",            110,11),
        new SkinMeta("skin_rose_gold",       "Rose Gold",         120,12),
        new SkinMeta("skin_prism",           "Prism",             130,13),
        new SkinMeta("skin_sakura",          "Sakura",            130,13),
        new SkinMeta("skin_koi",             "Koi",               140,14),
        new SkinMeta("skin_meteor",          "Meteor",            140,14),
        new SkinMeta("skin_starry",          "Starry Night",      150,14),
        new SkinMeta("skin_borealis",        "Borealis",          160,15),
        new SkinMeta("skin_cyberpunk",       "Cyberpunk",         160,15),
        new SkinMeta("skin_phantom",         "Phantom",           170,15),
        new SkinMeta("skin_runic",           "Runic",             170,15),
        new SkinMeta("skin_ember",           "Ember",             180,16),
        new SkinMeta("skin_crystal_amethyst","Crystal Amethyst",  180,16),
        new SkinMeta("skin_pearl",           "Pearl",             190,17),
        new SkinMeta("skin_toxic",           "Toxic",             190,17),
        new SkinMeta("skin_sandstorm",       "Sandstorm",         200,18),
        new SkinMeta("skin_circuit_neon",    "Circuit Neon",      210,18),
        new SkinMeta("skin_dragon_onyx",     "Onyx Dragon",       240,19),
        new SkinMeta("skin_phoenix_ashen",   "Ashen Phoenix",     240,19),
        new SkinMeta("skin_void_prism",      "Void Prism",        260,20),
        new SkinMeta("skin_aurora_pastel",   "Aurora Pastel",     260,20),

    };

    // 보상/레벨 해금 로직에서 사용하는 기본 카탈로그(레벨 필터는 호출부에서)
    public static IEnumerable<SkinMeta> UnlockByLevel => AllSkins;

    public static SnakeSkin SkinOf(string id) => id switch
    {
        "skin_basic_blue" => new("#2da8ff", "none", "none", "solid"),
        "skin_basic_green" => new("#28d17c", "none", "none", "solid"),
        "skin_basic_yellow" => new("#f7d21a", "none", "none", "solid"),
        "skin_basic_red" => new("#ff4d4d", "none", "none", "solid"),
        "skin_stripe_red" => new("#ff3a3a", "none", "none", "stripe"),
        "skin_dot_purple" => new("#8a56ff", "none", "none", "dot"),
        "skin_arrow_gold" => new("#ffcc33", "arrow", "ribbon", "solid"),
        "skin_royal_black" => new("#111111", "horn", "ribbon", "stripe"),

        "skin_neon_cyan" => new("#00e5ff", "none", "ribbon", "solid"),
        "skin_neon_magenta" => new("#ff2dac", "none", "ribbon", "solid"),
        "skin_ice_crystal" => new("#7bdff2", "horn", "none", "dot"),
        "skin_fire_flame" => new("#ff7e5f", "arrow", "none", "stripe"),
        "skin_tiger" => new("#e67e22", "horn", "none", "stripe"),
        "skin_panther" => new("#2c3e50", "horn", "none", "solid"),
        "skin_carbon" => new("#2b2b2b", "none", "none", "dot"),
        "skin_circuit" => new("#16a085", "arrow", "ribbon", "stripe"),
        "skin_camo_forest" => new("#3b6b3b", "none", "none", "stripe"),
        "skin_camo_desert" => new("#c2a66d", "none", "none", "stripe"),

        "skin_rainbow" => new("#ffffff", "none", "ribbon", "rainbow"),
        "skin_dragon" => new("#c0392b", "horn", "ribbon", "scale"),
        "skin_phoenix" => new("#d35400", "horn", "ribbon", "feather"),
        "skin_void" => new("#0b0b17", "horn", "ribbon", "solid"),
        "skin_aurora" => new("#98fbff", "none", "ribbon", "gradient"),


        "skin_stripe_teal" => new("#14b8a6", "none", "none", "stripe"),
        "skin_dot_lime" => new("#84cc16", "none", "none", "dot"),
        "skin_honeycomb" => new("#f59e0b", "horn", "none", "dot"),
        "skin_metal_chrome" => new("#cbd5e1", "none", "ribbon", "solid"),
        "skin_obsidian" => new("#0f172a", "horn", "none", "solid"),
        "skin_glacier" => new("#7dd3fc", "none", "none", "dot"),
        "skin_lava" => new("#ef4444", "arrow", "none", "stripe"),
        "skin_thunder" => new("#fde047", "arrow", "ribbon", "stripe"),
        "skin_shadow" => new("#111827", "horn", "none", "solid"),
        "skin_rose_gold" => new("#f5b6a4", "none", "ribbon", "solid"),
        "skin_prism" => new("#ffffff", "none", "ribbon", "rainbow"),
        "skin_sakura" => new("#f9a8d4", "none", "none", "dot"),
        "skin_koi" => new("#f97316", "horn", "none", "stripe"),
        "skin_meteor" => new("#a78bfa", "arrow", "none", "dot"),
        "skin_starry" => new("#1d4ed8", "none", "ribbon", "gradient"),
        "skin_borealis" => new("#22d3ee", "none", "ribbon", "gradient"),
        "skin_cyberpunk" => new("#06b6d4", "arrow", "ribbon", "stripe"),
        "skin_phantom" => new("#64748b", "horn", "none", "solid"),
        "skin_runic" => new("#10b981", "horn", "ribbon", "stripe"),
        "skin_ember" => new("#fb7185", "arrow", "none", "stripe"),
        "skin_crystal_amethyst" => new("#a855f7", "horn", "ribbon", "dot"),
        "skin_pearl" => new("#f1f5f9", "none", "ribbon", "solid"),
        "skin_toxic" => new("#84cc16", "horn", "none", "stripe"),
        "skin_sandstorm" => new("#facc15", "none", "none", "dot"),
        "skin_circuit_neon" => new("#22c55e", "arrow", "ribbon", "stripe"),
        "skin_dragon_onyx" => new("#0b0b17", "horn", "ribbon", "scale"),
        "skin_phoenix_ashen" => new("#f59e0b", "horn", "ribbon", "feather"),
        "skin_void_prism" => new("#0ea5e9", "horn", "ribbon", "rainbow"),
        "skin_aurora_pastel" => new("#93c5fd", "none", "ribbon", "gradient"),

        _ => new("#9e9e9e", "none", "none", "solid"),
    };

    public static string DisplayOf(string id)
    {
        var meta = AllSkins.FirstOrDefault(s => s.Id == id);
        return meta?.Display ?? id;
    }

    public static CosmeticInstance InstanceOf(string id)
        => new(id, DisplayOf(id), SkinOf(id));
}
