// Snake.Client.WinForms/OnlineShopForm.cs
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using Microsoft.AspNetCore.SignalR.Client;
using Snake.Shared;

namespace Snake.Client.WinForms
{
    public class OnlineShopForm : Form
    {
        private readonly HubConnection _conn;
        private readonly string _playerName;

        // 상단 재화 표시
        private readonly Label lblCoins = new() { Dock = DockStyle.Right, AutoSize = true, Padding = new Padding(0, 8, 12, 0) };
        private readonly Label lblShards = new() { Dock = DockStyle.Right, AutoSize = true, Padding = new Padding(0, 8, 12, 0) };

        // 탭 & 네비게이션
        private readonly TabControl tabs = new() { Dock = DockStyle.Fill };
        //private readonly ToolStrip nav = new() { GripStyle = ToolStripGripStyle.Hidden, Dock = DockStyle.Top };
        //private readonly ToolStripButton navGacha = new("가챠");
        //private readonly ToolStripButton navCraft = new("제작");
        //private readonly ToolStripButton navSkin = new("스킨 상점");
        //private readonly ToolStripButton navTheme = new("테마 상점");
        //private readonly ToolStripButton navHelp = new("Help");

        // 가챠 탭
        private readonly ListBox lbBanners = new() { Dock = DockStyle.Left, Width = 220 };
        private readonly FlowLayoutPanel pnlItems = new() { Dock = DockStyle.Fill, AutoScroll = true, Padding = new Padding(8) };
        private readonly Button btnPull1 = new() { Text = "1회(50c)", Width = 120, Height = 34 };
        private readonly Button btnPull10 = new() { Text = "10연(450c)", Width = 120, Height = 34 };
        private readonly FlowLayoutPanel pnlResults = new() { Dock = DockStyle.Bottom, Height = 160, AutoScroll = true, Padding = new Padding(8), WrapContents = true };

        // 제작 탭
        private readonly TextBox tbSearch = new() { PlaceholderText = "아이템 검색...", Dock = DockStyle.Top };
        private readonly FlowLayoutPanel pnlCraft = new() { Dock = DockStyle.Fill, AutoScroll = true, Padding = new Padding(8) };

        // 데이터
        private List<GachaBannerDto> _banners = new();
        private GachaBannerDto? _current;

        // 온라인 상점 전용 데이터
        private int _level = 1;
        private int _coins = 0;
        private List<ShopItemDto> _skinCatalog = new();
        private List<ShopItemDto> _themeCatalog = new();

        // 스킨/테마 탭 UI
        private readonly FlowLayoutPanel pnlSkins = new() { Dock = DockStyle.Fill, AutoScroll = true, Padding = new Padding(8), WrapContents = true };
        private readonly FlowLayoutPanel pnlThemes = new() { Dock = DockStyle.Fill, AutoScroll = true, Padding = new Padding(8), WrapContents = true };

        public OnlineShopForm(HubConnection conn, string playerName)
        {
            _conn = conn;
            _playerName = playerName;

            Text = "온라인 상점";
            StartPosition = FormStartPosition.CenterParent;
            Width = 900; Height = 680;

            // 상단 바(코인/샤드)
            var top = new Panel { Dock = DockStyle.Top, Height = 36 };
            top.Controls.Add(lblShards);
            top.Controls.Add(lblCoins);

            // ───────── 탭 설정(헤더 잘림 방지) ─────────
            tabs.Alignment = TabAlignment.Top;
            tabs.SizeMode = TabSizeMode.Fixed;
            tabs.ItemSize = new Size(120, 32);
            tabs.Padding = new Point(16, 6);

            // ───────── 네비게이션 바 ─────────
            //nav.Items.AddRange(new ToolStripItem[] { navGacha, navCraft, navSkin, navTheme, new ToolStripSeparator(), navHelp });
            //navGacha.Click += (_, __) => tabs.SelectedIndex = 0;
            //navCraft.Click += (_, __) => tabs.SelectedIndex = 1;
            //navSkin.Click += (_, __) => tabs.SelectedIndex = 2;
            //navTheme.Click += (_, __) => tabs.SelectedIndex = 3;
            //navHelp.Click += (_, __) => tabs.SelectedIndex = 4;

            // ───────── 탭 구성 ─────────
            var tabGacha = new TabPage("가챠");
            var tabCraft = new TabPage("제작");
            var tabSkins = new TabPage("스킨 상점");
            var tabThemes = new TabPage("테마 상점");
            var tabHelp = new TabPage("Help");

            // ─ Gacha 탭 레이아웃
            var left = new Panel { Dock = DockStyle.Left, Width = 220, Padding = new Padding(8, 8, 0, 8) };
            left.Controls.Add(lbBanners);

            // 가운데 컨테이너(Left와 분리)
            var center = new Panel { Dock = DockStyle.Fill };

            // 상단 라벨/버튼 바
            var gTop = new Panel { Dock = DockStyle.Top, Height = 42, Padding = new Padding(8) };
            gTop.Controls.Add(new Label { Text = "배너 풀", AutoSize = true, Dock = DockStyle.Left, Padding = new Padding(0, 12, 0, 0) });

            var gBtns = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 42, FlowDirection = FlowDirection.LeftToRight, Padding = new Padding(8) };
            gBtns.Controls.Add(btnPull1);
            gBtns.Controls.Add(btnPull10);

            // 가운데 컨테이너에 도킹 순서 중요: Fill 먼저 추가 → Top들 → Bottom
            center.SuspendLayout();
            center.Controls.Add(pnlItems);     // Fill
            center.Controls.Add(gBtns);        // Top
            center.Controls.Add(gTop);         // Top
            center.Controls.Add(pnlResults);   // Bottom
            center.ResumeLayout();

            // 탭 루트에는 Left와 Fill만 배치 (교차 도킹 금지)
            tabGacha.SuspendLayout();
            tabGacha.Controls.Clear();
            tabGacha.Controls.Add(center); // Fill
            tabGacha.Controls.Add(left);   // Left
            tabGacha.ResumeLayout();

            pnlResults.MinimumSize = new Size(0, 150);

            // ─ Craft 탭
            tabCraft.Controls.Add(pnlCraft);
            tabCraft.Controls.Add(tbSearch);

            // ─ 온라인 상점(스킨/테마)
            tabSkins.Controls.Add(pnlSkins);
            tabThemes.Controls.Add(pnlThemes);

            // ─ Help 탭
            var help = new RichTextBox
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                BorderStyle = BorderStyle.None,
                DetectUrls = false,
                ScrollBars = RichTextBoxScrollBars.Vertical,
                Text =
@"[온라인 상점 가이드]

■ 재화
- 코인: 매치 보상으로 얻으며 가챠/상점 구매에 사용합니다.
- 샤드: 가챠에서 '중복'이 나올 때 희귀도에 따라 환급되는 파편입니다. 제작에 사용합니다.
  · 환급량(희귀도별): Common 10 / Rare 25 / Epic 75 / Legendary 250

■ 가챠
- 1회 50c / 10연 450c.
- 확률(%) Common 62 / Rare 25 / Epic 10 / Legendary 3
- 피티: 10연 ≥ Rare 보장, 30연 ≥ Epic 보장.

■ 제작
- 샤드로 원하는 아이템을 즉시 제작합니다.
- 비용: Common 120 / Rare 300 / Epic 600 / Legendary 900
- 이미 보유한 아이템은 제작 불가.

■ 스킨/테마 상점
- 카드에 '요구 레벨/가격/보유여부'가 표시됩니다.
- 조건 미달(레벨/코인)은 버튼 비활성 및 툴팁로 사유 표시."
            };
            tabHelp.Controls.Add(help);

            // 탭 추가
            tabs.TabPages.Add(tabGacha);
            tabs.TabPages.Add(tabCraft);
            tabs.TabPages.Add(tabSkins);
            tabs.TabPages.Add(tabThemes);
            tabs.TabPages.Add(tabHelp);

            // ── ⬇⬇⬇ 중요한 부분: 도킹/추가 순서(겹침 방지) ⬇⬇⬇
            SuspendLayout();
            Controls.Add(tabs); // Fill 먼저 추가
            //Controls.Add(nav);  // Top
            Controls.Add(top);  // Top(최상단)
            ResumeLayout(performLayout: true);
            // ── ⬆⬆⬆ 여기까지 순서가 핵심입니다 ⬆⬆⬆

            // 이벤트
            Load += async (_, __) => await InitAsync();
            lbBanners.SelectedIndexChanged += (_, __) => OnSelectBanner();
            btnPull1.Click += async (_, __) => await DoPullAsync(1);
            btnPull10.Click += async (_, __) => await DoPullAsync(10);
            tbSearch.TextChanged += (_, __) => RebuildCraftGrid();
        }

        // ------------------------ Data Init ------------------------
        private async System.Threading.Tasks.Task InitAsync()
        {
            await RefreshCoinsAndShardsAsync();      // coins/shards
            await RefreshLevelAndCatalogsAsync();    // level + catalogs

            try
            {
                _banners = await _conn.InvokeAsync<List<GachaBannerDto>>("GetGachaBanners");
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, $"배너 불러오기 실패: {ex.Message}", "온라인 상점");
                _banners = new();
            }

            lbBanners.Items.Clear();
            foreach (var b in _banners.OrderBy(x => x.StartUtc))
                lbBanners.Items.Add($"{b.Title}  ({b.StartUtc:MM/dd}~{b.EndUtc:MM/dd})");

            var hasBanner = _banners.Count > 0;
            btnPull1.Enabled = btnPull10.Enabled = hasBanner;
            if (hasBanner) lbBanners.SelectedIndex = 0;

            RebuildCraftGrid();
            RebuildSkinGrid();
            RebuildThemeGrid();
        }

        private async System.Threading.Tasks.Task RefreshCoinsAndShardsAsync()
        {
            try
            {
                var inv = await _conn.InvokeAsync<InventoryDto>("GetInventory", _playerName);
                _coins = inv.Coins;
                lblCoins.Text = $"코인: {_coins}c";
            }
            catch { lblCoins.Text = "코인: -"; }

            try
            {
                var shard = await _conn.InvokeAsync<ShardInfoDto>("GetShards", _playerName);
                lblShards.Text = $"샤드: {shard.Shards}";
            }
            catch { lblShards.Text = "샤드: -"; }
        }

        private async System.Threading.Tasks.Task RefreshLevelAndCatalogsAsync()
        {
            try
            {
                var prof = await _conn.InvokeAsync<AccountProfileDto>("GetAccountProfile", _playerName);
                _level = Math.Max(1, prof.Level);
            }
            catch { _level = 1; }

            try { _skinCatalog = await _conn.InvokeAsync<List<ShopItemDto>>("GetCosmeticCatalog", _playerName); }
            catch { _skinCatalog = new(); }

            try { _themeCatalog = await _conn.InvokeAsync<List<ShopItemDto>>("GetThemeCatalog", _playerName); }
            catch { _themeCatalog = new(); }
        }

        // ------------------------ Gacha UI ------------------------
        private void OnSelectBanner()
        {
            var ix = lbBanners.SelectedIndex;
            _current = (ix >= 0 && ix < _banners.Count) ? _banners[ix] : null;

            pnlItems.SuspendLayout();
            pnlItems.Controls.Clear();

            if (_current == null)
            {
                pnlItems.Controls.Add(new Label { Text = "선택된 배너가 없습니다.", AutoSize = true });
                btnPull1.Enabled = btnPull10.Enabled = false;
            }
            else
            {
                foreach (var g in _current.Pool.OrderByDescending(x => x.Rarity).ThenBy(x => x.Display))
                    pnlItems.Controls.Add(MakeItemCard(g));
                btnPull1.Enabled = btnPull10.Enabled = true;
            }

            pnlItems.ResumeLayout();
        }

        private Control MakeItemCard(GachaItemDto item)
        {
            var card = new Panel
            {
                Width = 230,
                Height = 90,
                Margin = new Padding(6),
                BorderStyle = BorderStyle.FixedSingle,
                BackColor = Color.White
            };
            var title = new Label
            {
                Text = item.Display,
                AutoSize = true,
                Left = 8,
                Top = 8,
                Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold),
                ForeColor = RarityColor(item.Rarity)
            };
            var info = new Label
            {
                Text = $"종류: {item.Kind} / 희귀도: {item.Rarity}",
                AutoSize = true,
                Left = 8,
                Top = 34
            };
            var id = new Label { Text = item.Id, AutoSize = true, Left = 8, Top = 56, ForeColor = Color.DimGray };

            card.Controls.Add(title);
            card.Controls.Add(info);
            card.Controls.Add(id);
            return card;
        }

        private async System.Threading.Tasks.Task DoPullAsync(int count)
        {
            if (_current == null) return;

            try
            {
                var res = await _conn.InvokeAsync<GachaPullResult>("PullGacha",
                    new GachaPullRequest(_playerName, _current.BannerId, count));

                await RefreshCoinsAndShardsAsync();
                ShowResults(res);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, $"뽑기 실패: {ex.Message}", "가챠");
            }
        }

        private void ShowResults(GachaPullResult res)
        {
            pnlResults.SuspendLayout();
            pnlResults.Controls.Clear();

            var summary = new Label
            {
                AutoSize = true,
                Text = $"결과: {res.Results.Count}개  /  코인 -{res.SpentCoins}c  /  샤드 {(res.ShardsDelta >= 0 ? "+" : "")}{res.ShardsDelta}  /  잔액 {res.RemainingCoins}c"
            };
            pnlResults.Controls.Add(summary);

            foreach (var it in res.Results)
            {
                var p = new Panel
                {
                    Width = 180,
                    Height = 68,
                    Margin = new Padding(6),
                    BorderStyle = BorderStyle.FixedSingle,
                    BackColor = Color.White
                };
                var name = _current?.Pool.FirstOrDefault(x => x.Id == it.Id)?.Display ?? it.Id;
                var title = new Label
                {
                    Text = name,
                    AutoSize = true,
                    Left = 8,
                    Top = 8,
                    Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold),
                    ForeColor = RarityColor(it.Rarity)
                };
                var sub = new Label
                {
                    Text = it.IsNew ? "신규 획득!" : $"중복 → 샤드 +{it.ShardGain}",
                    AutoSize = true,
                    Left = 8,
                    Top = 34
                };
                p.Controls.Add(title);
                p.Controls.Add(sub);
                pnlResults.Controls.Add(p);
            }

            pnlResults.ResumeLayout();
        }

        // ------------------------ Online Skin/Theme Shop ------------------------
        private void RebuildSkinGrid()
        {
            pnlSkins.SuspendLayout();
            pnlSkins.Controls.Clear();
            foreach (var it in _skinCatalog
                .OrderBy(x => x.MinLevel)
                .ThenBy(x => x.Price)
                .ThenBy(x => x.Display, StringComparer.CurrentCultureIgnoreCase))
            {
                pnlSkins.Controls.Add(MakeOnlineShopCard(it, isTheme: false));
            }
            pnlSkins.ResumeLayout();
        }

        private void RebuildThemeGrid()
        {
            pnlThemes.SuspendLayout();
            pnlThemes.Controls.Clear();
            foreach (var it in _themeCatalog
                .OrderBy(x => x.MinLevel)
                .ThenBy(x => x.Price)
                .ThenBy(x => x.Display, StringComparer.CurrentCultureIgnoreCase))
            {
                pnlThemes.Controls.Add(MakeOnlineShopCard(it, isTheme: true));
            }
            pnlThemes.ResumeLayout();
        }

        private Control MakeOnlineShopCard(ShopItemDto it, bool isTheme)
        {
            var card = new Panel
            {
                Width = 280,
                Height = 120,
                Margin = new Padding(6),
                BorderStyle = BorderStyle.FixedSingle,
                BackColor = Color.White
            };

            var title = new Label
            {
                Text = it.Display,
                AutoSize = true,
                Left = 10,
                Top = 8,
                Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold)
            };
            var id = new Label { Text = $"ID: {it.Id}", AutoSize = true, Left = 10, Top = 32, ForeColor = Color.DimGray };
            var price = new Label { Text = $"가격: {it.Price}c", AutoSize = true, Left = 10, Top = 52 };
            var req = new Label { Text = $"요구 레벨: {it.MinLevel}", AutoSize = true, Left = 140, Top = 52, ForeColor = Color.SlateGray };
            var btn = new Button { Text = it.Owned ? "보유중" : "구매", Left = 10, Top = 80, Width = 80 };

            var canBuy = !it.Owned && (_level >= it.MinLevel) && (_coins >= it.Price);
            if (it.Owned)
            {
                btn.Enabled = false;
                btn.BackColor = Color.Gainsboro;
            }
            else
            {
                btn.Enabled = canBuy;
                if (!canBuy)
                {
                    btn.BackColor = Color.MistyRose;
                    var reason = (_level < it.MinLevel ? $"레벨 {it.MinLevel} 필요" : "코인 부족");
                    new ToolTip().SetToolTip(btn, reason);
                }
            }

            btn.Click += async (_, __) =>
            {
                try
                {
                    var res = isTheme
                        ? await _conn.InvokeAsync<PurchaseResultDto>("BuyTheme", _playerName, it.Id)
                        : await _conn.InvokeAsync<PurchaseResultDto>("BuyCosmetic", _playerName, it.Id);

                    MessageBox.Show(this, res.Message, res.Ok ? "구매 완료" : "구매 실패");
                    await RefreshCoinsAndShardsAsync();
                    await RefreshLevelAndCatalogsAsync();
                    RebuildSkinGrid();
                    RebuildThemeGrid();
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message, "구매 오류");
                }
            };

            if (_level < it.MinLevel)
            {
                title.ForeColor = Color.Gray;
                price.ForeColor = Color.IndianRed;
                req.ForeColor = Color.IndianRed;
            }
            if (_coins < it.Price) price.ForeColor = Color.IndianRed;

            card.Controls.Add(title);
            card.Controls.Add(id);
            card.Controls.Add(price);
            card.Controls.Add(req);
            card.Controls.Add(btn);
            return card;
        }

        // ------------------------ Craft UI ------------------------
        private void RebuildCraftGrid()
        {
            pnlCraft.SuspendLayout();
            pnlCraft.Controls.Clear();

            var pool = _banners.SelectMany(b => b.Pool).GroupBy(x => x.Id).Select(g => g.First()).ToList();
            var q = tbSearch.Text.Trim();
            if (q.Length > 0) pool = pool.Where(x => x.Display.Contains(q, StringComparison.OrdinalIgnoreCase)).ToList();

            foreach (var it in pool.OrderByDescending(x => x.Rarity).ThenBy(x => x.Display))
                pnlCraft.Controls.Add(MakeCraftCard(it));

            pnlCraft.ResumeLayout();
        }

        private Control MakeCraftCard(GachaItemDto item)
        {
            var cost = item.Rarity switch
            {
                Rarity.Common => 120,
                Rarity.Rare => 300,
                Rarity.Epic => 600,
                Rarity.Legendary => 900,
                _ => 300
            };

            var card = new Panel
            {
                Width = 260,
                Height = 110,
                Margin = new Padding(6),
                BorderStyle = BorderStyle.FixedSingle,
                BackColor = Color.White
            };
            var title = new Label
            {
                Text = item.Display,
                AutoSize = true,
                Left = 8,
                Top = 8,
                Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold),
                ForeColor = RarityColor(item.Rarity)
            };
            var info = new Label
            {
                Text = $"희귀도: {item.Rarity}  /  샤드 비용: {cost}",
                AutoSize = true,
                Left = 8,
                Top = 34
            };
            var btn = new Button { Text = "제작", Left = 8, Top = 66, Width = 72 };
            btn.Click += async (_, __) =>
            {
                try
                {
                    var res = await _conn.InvokeAsync<CraftResult>("Craft", new CraftRequest(_playerName, item.Id));
                    MessageBox.Show(this, res.Ok ? $"[{item.Display}] 제작 완료! (샤드 -{res.CostShards})" : res.Message, "제작");
                    await RefreshCoinsAndShardsAsync();
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, $"제작 실패: {ex.Message}", "제작");
                }
            };

            card.Controls.Add(title);
            card.Controls.Add(info);
            card.Controls.Add(btn);
            return card;
        }

        private static Color RarityColor(Rarity r) => r switch
        {
            Rarity.Common => Color.DimGray,
            Rarity.Rare => Color.DodgerBlue,
            Rarity.Epic => Color.MediumOrchid,
            Rarity.Legendary => Color.Goldenrod,
            _ => Color.Black
        };
    }
}
