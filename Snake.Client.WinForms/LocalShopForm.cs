// Snake.Client.WinForms/LocalShopForm.cs
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using Snake.Shared; // CosmeticCatalog, ThemeCatalog

namespace Snake.Client.WinForms
{
    public class LocalShopForm : Form
    {
        private readonly Func<int> _getCoins;
        private readonly Action<int> _setCoins;
        private readonly Func<string, int, bool, bool> _tryBuy; // (itemId, price, isTheme) => success

        private readonly Label lblCoins = new() { Dock = DockStyle.Left, AutoSize = true, Padding = new Padding(0, 8, 8, 0) };
        private readonly TextBox tbSearch = new() { Dock = DockStyle.Fill, PlaceholderText = "검색(이름/ID)" };
        private readonly ComboBox cbSort = new() { Dock = DockStyle.Right, Width = 140, DropDownStyle = ComboBoxStyle.DropDownList };

        private readonly TabControl tabs = new() { Dock = DockStyle.Fill };
        private readonly FlowLayoutPanel panelSkins = new() { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight, WrapContents = true, Padding = new Padding(10), AutoScroll = true };
        private readonly FlowLayoutPanel panelThemes = new() { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight, WrapContents = true, Padding = new Padding(10), AutoScroll = true };

        // 표기가격(없으면 기본가 사용, 해시 제거 → 고정 가격)
        private readonly Dictionary<string, int> _price = new(StringComparer.OrdinalIgnoreCase)
        {
            ["skin_basic_blue"] = 120,
            ["skin_basic_green"] = 120,
            ["skin_basic_red"] = 150,
            ["theme_dark"] = 100,
            ["theme_retro"] = 140,
            ["theme_neon"] = 160,
        };

        public LocalShopForm(Func<int> getCoins, Action<int> setCoins, Func<string, int, bool, bool> tryBuy)
        {
            _getCoins = getCoins;
            _setCoins = setCoins;
            _tryBuy = tryBuy;

            Text = "로컬 상점";
            StartPosition = FormStartPosition.CenterParent;
            Width = 820; Height = 520;

            // 상단 바
            var top = new Panel { Dock = DockStyle.Top, Height = 40, Padding = new Padding(10, 4, 10, 4) };
            cbSort.Items.AddRange(new object[] { "이름↑", "이름↓", "가격↑", "가격↓" });
            cbSort.SelectedIndex = 0;

            var right = new Panel { Dock = DockStyle.Right, Width = 160 };
            right.Controls.Add(cbSort);
            top.Controls.Add(right);
            top.Controls.Add(tbSearch);
            top.Controls.Add(lblCoins);

            // 탭
            var tpSkins = new TabPage("스킨"); tpSkins.Controls.Add(panelSkins);
            var tpThemes = new TabPage("테마"); tpThemes.Controls.Add(panelThemes);
            tabs.TabPages.Add(tpSkins);
            tabs.TabPages.Add(tpThemes);

            Controls.Add(tabs);
            Controls.Add(top);

            tbSearch.TextChanged += (_, __) => RebuildPanels();
            cbSort.SelectedIndexChanged += (_, __) => RebuildPanels();

            UpdateCoinsText();
            RebuildPanels();
        }

        private void UpdateCoinsText() => lblCoins.Text = $"보유 코인: {_getCoins()}c";

        private int GetPriceOrDefault(string id)
        {
            // 고정 가격(세션마다 달라지는 해시 제거)
            return _price.TryGetValue(id, out var p) ? p : 120;
        }

        private void RebuildPanels()
        {
            string q = tbSearch.Text?.Trim() ?? "";

            // 스킨
            var skinItems = CosmeticCatalog.AllSkins
                .Select(s => new ShopItem(s.Id, s.Display, GetPriceOrDefault(s.Id), IsTheme: false))
                .Where(it => Filter(it, q))
                .ToList();

            // 테마
            var themeItems = ThemeCatalog.AllThemes
                .Select(t => new ShopItem(t.Id, t.Display, GetPriceOrDefault(t.Id), IsTheme: true))
                .Where(it => Filter(it, q))
                .ToList();

            skinItems = Sort(skinItems);
            themeItems = Sort(themeItems);

            panelSkins.SuspendLayout();
            panelSkins.Controls.Clear();
            foreach (var it in skinItems) panelSkins.Controls.Add(MakeItemCard(it));
            panelSkins.ResumeLayout();

            panelThemes.SuspendLayout();
            panelThemes.Controls.Clear();
            foreach (var it in themeItems) panelThemes.Controls.Add(MakeItemCard(it));
            panelThemes.ResumeLayout();
        }

        private bool Filter(ShopItem it, string q)
        {
            if (string.IsNullOrWhiteSpace(q)) return true;
            return it.Display.IndexOf(q, StringComparison.OrdinalIgnoreCase) >= 0
                || it.Id.IndexOf(q, StringComparison.OrdinalIgnoreCase) >= 0;
        }

        private List<ShopItem> Sort(List<ShopItem> src)
        {
            return cbSort.SelectedIndex switch
            {
                0 => src.OrderBy(x => x.Display, StringComparer.CurrentCultureIgnoreCase).ToList(), // 이름↑
                1 => src.OrderByDescending(x => x.Display, StringComparer.CurrentCultureIgnoreCase).ToList(), // 이름↓
                2 => src.OrderBy(x => x.Price).ToList(), // 가격↑
                3 => src.OrderByDescending(x => x.Price).ToList(), // 가격↓
                _ => src
            };
        }

        private Panel MakeItemCard(ShopItem it)
        {
            var card = new Panel
            {
                Width = 250,
                Height = 120,
                Margin = new Padding(6),
                BorderStyle = BorderStyle.FixedSingle
            };

            var name = new Label { Text = it.Display, Left = 10, Top = 10, AutoSize = true, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold) };
            var id = new Label { Text = $"ID: {it.Id}", Left = 10, Top = 32, AutoSize = true, ForeColor = Color.DimGray };
            var price = new Label { Text = $"가격: {it.Price}c", Left = 10, Top = 52, AutoSize = true };
                        // 요구 레벨(오프라인: 참고용)
            int minLv = it.IsTheme
                            ? ThemeCatalog.AllThemes.FirstOrDefault(t => t.Id == it.Id)?.MinLevel ?? 1
                            : CosmeticCatalog.AllSkins.FirstOrDefault(s => s.Id == it.Id)?.MinLevel ?? 1;
            var req = new Label { Text = $"요구 레벨: {minLv} (오프라인은 참고용)", Left = 120, Top = 52, AutoSize = true, ForeColor = Color.SlateGray };
            var btn = new Button { Text = "구매", Left = 10, Top = 80, Width = 70 };

            btn.Click += (_, __) =>
            {
                var coins = _getCoins();
                if (coins < it.Price)
                {
                    MessageBox.Show(this, "코인이 부족합니다.", "상점", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                // 실제 검증은 _tryBuy에서 처리(중복 보유 포함)
                if (_tryBuy(it.Id, it.Price, it.IsTheme))
                {
                    UpdateCoinsText();
                    MessageBox.Show(this, $"[{it.Display}] 구매 완료!", "상점", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else
                {
                    MessageBox.Show(this, "이미 보유한 아이템입니다.", "상점", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            };

            // 여유자금 안내(표시용)
            int coins = _getCoins();
            if (coins < it.Price)
            {
                price.ForeColor = Color.IndianRed;
                var tip = new ToolTip(); tip.SetToolTip(price, "코인이 부족합니다.");
            }

            card.Controls.Add(name);
            card.Controls.Add(id);
            card.Controls.Add(price);
            card.Controls.Add(btn);
            card.Controls.Add(req);
            return card;
        }

        private record ShopItem(string Id, string Display, int Price, bool IsTheme);
    }
}
