// Snake.Client.WinForms/UnifiedMainForm.cs
using Microsoft.AspNetCore.SignalR.Client;
using Snake.Shared;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Media;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;
using Timer = System.Windows.Forms.Timer;

namespace Snake.Client.WinForms;

public class UnifiedMainForm : Form
{

// === Toast / FloatingText ===
sealed class Toast
{
    public DateTime Until { get; init; }
    public string Text { get; init; } = string.Empty; // NRT 안전 기본값
}
readonly Queue<Toast> _toasts = new();
void PushToast(string text) { _toasts.Enqueue(new Toast{ Text=text, Until=DateTime.UtcNow.AddSeconds(2)}); lblToast.Text = text; lblToast.Visible = true; tToast.Start(); }
readonly Timer tToast = new(){ Interval = 800 };
readonly Label lblToast = new(){ Dock = DockStyle.Bottom, Height = 24, TextAlign = ContentAlignment.MiddleCenter, Visible = false };

readonly ToolStrip tool = new() { GripStyle = ToolStripGripStyle.Hidden, RenderMode = ToolStripRenderMode.System };
    readonly ToolStripButton btnRefreshRooms = new("방 새로고침") { DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnConnect = new("접속") { DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnJoin = new("입장") { DisplayStyle =
        ToolStripItemDisplayStyle.Text, Enabled = false };
    readonly ToolStripButton btnLeave = new("나가기") { DisplayStyle = ToolStripItemDisplayStyle.Text, Enabled = false };
    readonly ToolStripButton btnReady = new("Ready") { CheckOnClick = true, DisplayStyle = ToolStripItemDisplayStyle.Text, Enabled = false };
    readonly ToolStripButton btnStart = new("시작(방장)") { DisplayStyle = ToolStripItemDisplayStyle.Text, Enabled = false };
    readonly ToolStripButton btnCreateRoom = new("방 만들기") { DisplayStyle = ToolStripItemDisplayStyle.Text, Enabled = false };
    readonly ToolStripButton btnSolo = new("싱글플레이") { CheckOnClick = true, DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnShop = new("상점") { DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnChangeNick = new("닉변경") { DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnHelp = new("Help") { DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnSkip = new("대기 스킵(방장)") { DisplayStyle = ToolStripItemDisplayStyle.Text, Enabled = false };


    readonly ToolStripSeparator sepSolo = new();
    readonly ToolStripButton btnNew = new("새로하기") { DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnSave = new("저장하기") { DisplayStyle = ToolStripItemDisplayStyle.Text };
    readonly ToolStripButton btnLoad = new("불러오기") { DisplayStyle = ToolStripItemDisplayStyle.Text };

    readonly ToolStripSeparator sep1 = new();
    readonly ToolStripSeparator sep2 = new();
    readonly ToolStripLabel spinLabel = new("연결중...");
    readonly ToolStripProgressBar spin = new() { Style = ProgressBarStyle.Marquee, MarqueeAnimationSpeed = 25 };

    // 화면엔 안 보이지만 내부 보관용
    readonly TextBox txtUrl = new() { Dock = DockStyle.Fill, Text = "http://localhost:5000/hub", Visible = false };
    readonly TextBox txtName = new() { Dock = DockStyle.Fill, Text = "", Visible = false };

    // 상단에서 Room 입력칸 제거 → Skin/Theme만 노출
    readonly ComboBox cmbSkin = new() { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList };
    readonly ComboBox cmbTheme = new() { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList };

    //readonly TabControl tabs = new() { Dock = DockStyle.Left, Width = 300 };
    readonly ListView lvRooms = new() { Dock = DockStyle.Fill, View = View.Details, FullRowSelect = true, MultiSelect = false };
    readonly ListView lvPlayers = new() { Dock = DockStyle.Fill, View = View.Details };
    readonly ListView lvScore = new() { Dock = DockStyle.Right, Width = 200, View = View.Details };

    readonly Panel canvas = new GameCanvas() { Dock = DockStyle.Fill, BackColor = Color.Black };

    readonly StatusStrip status = new();
    readonly ToolStripStatusLabel statL = new("Disconnected");
    readonly ToolStripStatusLabel statR = new("") { Spring = true };

    // 채팅 탭 컨테이너
    readonly TabControl chatTabs = new() { Dock = DockStyle.Fill };
    readonly ListBox lbLobby = new() { Dock = DockStyle.Fill };
    readonly TextBox tbLobby = new() { Dock = DockStyle.Bottom, PlaceholderText = "로비 채팅..." };
    readonly ListBox lbRoom = new() { Dock = DockStyle.Fill };
    readonly TextBox tbRoomChat = new() { Dock = DockStyle.Bottom, PlaceholderText = "방 채팅..." };
    readonly ListBox lbWhisper = new() { Dock = DockStyle.Fill };
    readonly TextBox tbWhisperTo = new() { Dock = DockStyle.Top, PlaceholderText = "상대 닉네임" };
    readonly TextBox tbWhisper = new() { Dock = DockStyle.Bottom, PlaceholderText = "귓속말..." };
    //readonly ListBox lbNotes = new() { Dock = DockStyle.Fill };
    //readonly TextBox tbNoteTo = new() { Dock = DockStyle.Top, PlaceholderText = "받는 사람" };
    //readonly TextBox tbNote = new() { Dock = DockStyle.Bottom, PlaceholderText = "쪽지 내용..." };
    //readonly Button btnLoadNotes = new() { Text = "쪽지함 불러오기", Dock = DockStyle.Bottom };

    // 플레이어 컨텍스트 메뉴
    readonly ContextMenuStrip ctxPlayers = new();
    readonly ToolStripMenuItem miInfo = new("정보 보기");
    readonly ToolStripMenuItem miWhisper = new("귓속말...");
    //readonly ToolStripMenuItem miNote = new("쪽지...");
    readonly ToolStripMenuItem miKick = new("강퇴(방장)");
    readonly ToolStripMenuItem miGiveHost = new("방장 위임...");

    HubConnection _conn = default!;
    string _playerId = "";
    GameSnapshot? _snap;
    bool _connected => _conn is not null && _conn.State == HubConnectionState.Connected;

    readonly Timer _render = new() { Interval = 16 };
    readonly Timer _soloTick = new() { Interval = 1000 / 12 };
    readonly Timer _roomsTimer = new() { Interval = 1000 };
    int _cell = 16;

    int _frames = 0, _fps = 0;
    readonly Timer _fpsTimer = new() { Interval = 1000 };
    bool _paused = false;

    // === 월드 크기/원점 보정 ===
    int _worldW = 48;
    int _worldH = 32;
    int _originX = 0;
    int _originY = 32;

    readonly SoundPlayer sndApple = SafeLoad("apple.wav");
    readonly SoundPlayer sndDead = SafeLoad("dead.wav");
    static SoundPlayer SafeLoad(string path) => File.Exists(path) ? new SoundPlayer(path) : new SoundPlayer();

    SoloEngine? _solo;
    bool _soloDeathShown = false;
    int _lastSoloScore = 0;
    int _lastSoloLevel = 1;

    int _soloCoins = 0;

    // 대소문자 무시 세트
    readonly HashSet<string> _ownedSkins = new(StringComparer.OrdinalIgnoreCase) { "skin_basic_blue", "skin_basic_green" };
    readonly HashSet<string> _ownedThemes = new(StringComparer.OrdinalIgnoreCase) { "theme_dark", "theme_retro", "theme_neon" };

    string _selectedSkinId = "skin_basic_blue";
    string _selectedThemeId = "theme_dark";
    const string SHOP_STATE_FILE = "solo_shop.json";

    int _onlineCoins = 0;
    HashSet<string> _onlineSkins = new(StringComparer.OrdinalIgnoreCase);
    HashSet<string> _onlineThemes = new(StringComparer.OrdinalIgnoreCase);
    string? _onlineSelectedSkinId;
    string? _onlineSelectedThemeId;

    readonly List<Floater> _floaters = new();

    // 로그인 정보
    string _loginId = "";
    string _loginPassword = "";

    // 탭-부스트(싱글)
    long _lastTapAtMs = 0;
    InputKey? _lastTapKey = null;
    int _tapStreak = 0;
    const int TAP_WINDOW_MS = 350;

    // 현재 입장 방ID
    string? _currentRoomId = null;

    // 콤보 채우는 동안 이벤트 무시
    bool _fillingCombos = false;

    // ★ 입장 중복 방지 플래그
    bool _isJoining = false;

    // ==== 추가: 호스트 관련 상태 ====
    bool _amIHost = false;
    string? _hostPlayerId = null;

    // 컨텍스트 대상/갱신 일시정지
    string? _ctxTargetName = null;
    bool _suspendPlayerListRefresh = false;


    // 로비 명단을 보여주는 중인지 여부
    bool _showingLobbyRoster = false;


    // 긴 허브 호출 중 재진입/타이머 중복 갱신 방지용
    bool _uiBusy = false;

    // 타이머/버튼 상태를 잠깐 묶는 헬퍼
    async Task RunUiBusy(Func<Task> work)
    {
        if (_uiBusy) return;
        _uiBusy = true;
        var oldCursor = Cursor.Current;
        Cursor.Current = Cursors.WaitCursor;
        var oldTimer = _roomsTimer.Enabled;
        _roomsTimer.Enabled = false;
        btnCreateRoom.Enabled = false;
        btnJoin.Enabled = false;

        try { await work(); }
        finally
        {
            _roomsTimer.Enabled = oldTimer;
            btnCreateRoom.Enabled = true;
            btnJoin.Enabled = true;
            Cursor.Current = oldCursor;
            _uiBusy = false;
        }
    }


    public UnifiedMainForm()
    {
            Controls.Add(lblToast);
            tToast.Tick += (_, __) => { if (_toasts.Count==0 || _toasts.Peek().Until < DateTime.UtcNow){ if(_toasts.Count>0)_toasts.Dequeue(); lblToast.Visible = _toasts.Count>0; if(_toasts.Count>0) lblToast.Text=_toasts.Peek().Text; else tToast.Stop(); } };
        Text = "Snake Multiplayer";
        Width = 1400; Height = 800; DoubleBuffered = true;

        // 로그인 폼
        Shown += async (_, __) =>
        {
            using var lf = new LoginForm();
            if (lf.ShowDialog(this) != DialogResult.OK) { Close(); return; }
            txtUrl.Text = lf.FinalUrl;
            _loginId = lf.LoginId;
            _loginPassword = lf.Password;
            await ConnectAsync(_loginId, _loginPassword);
        };

        _render.Tick += (_, __) =>
        {
            for (int i = _floaters.Count - 1; i >= 0; i--)
                if (!_floaters[i].Tick()) _floaters.RemoveAt(i);
            canvas.Invalidate(); _frames++;
        };
        _render.Start();
        _fpsTimer.Tick += (_, __) => { _fps = _frames; _frames = 0; statR.Text = $"FPS:{_fps}"; };
        _fpsTimer.Start();

        KeyPreview = true;
        Activated += (_, __) => canvas.Focus();
        canvas.TabStop = true;
        canvas.MouseDown += (_, __) => canvas.Focus();

        _soloTick.Tick += (_, __) =>
        {
            if (_paused || _solo is null) return;
            _solo.Update();
            _snap = _solo.Current;
            foreach (var ev in _solo.DrainEvents())
            {
                if (ev.Kind == LootKind.Gold) { _soloCoins += ev.Amount; SaveShopState(); AddFloater(ev.Cell, $"+{ev.Amount}c", LootKind.Gold); }
                else AddFloater(ev.Cell, $"+{ev.Amount}xp", LootKind.Xp);
            }
            if (_solo.Score > _lastSoloScore) { TryPlay(sndApple); _soloCoins += 2; _lastSoloScore = _solo.Score; SaveShopState(); }
            if (_solo.Level != _lastSoloLevel) { _lastSoloLevel = _solo.Level; ApplySoloSpeed(); }
            if (!_solo.Alive && !_soloDeathShown) { _soloDeathShown = true; TryPlay(sndDead); ShowSoloDeathPopup(); }
            UpdateSidebar();
        };

        tool.Items.AddRange(new ToolStripItem[]
        {
            btnRefreshRooms, sep1,
            btnConnect, btnJoin, btnLeave, btnReady, btnStart, btnSkip, btnCreateRoom,
            sep2, btnSolo, sepSolo, btnNew, btnSave, btnLoad,
            new ToolStripSeparator(), btnShop, btnChangeNick, new ToolStripSeparator(), btnHelp,
            new ToolStripSeparator(), spinLabel, spin
        });
        spin.Visible = spinLabel.Visible = false;

        // 상단(ROOM 제거 → Skin/Theme만)
        var top = new TableLayoutPanel { Dock = DockStyle.Top, Height = 40, ColumnCount = 4, Padding = new Padding(6, 4, 6, 0) };
        top.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        top.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        top.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        top.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));

        top.Controls.Add(new Label { Text = "Skin" }, 0, 0); top.Controls.Add(cmbSkin, 1, 0);
        top.Controls.Add(new Label { Text = "Theme" }, 2, 0); top.Controls.Add(cmbTheme, 3, 0);


        lvRooms.Columns.Add("Room", 160); lvRooms.Columns.Add("Players", 70); lvRooms.Columns.Add("Ready", 60); lvRooms.Columns.Add("Phase", 80);
        lvPlayers.Columns.Add("Player", 160); lvPlayers.Columns.Add("Lv", 40);
        lvPlayers.Columns.Add("상태", 120); // "로비" 또는 "방:1234" 같은 표기
        lvScore.Columns.Add("Rank", 50); lvScore.Columns.Add("Score", 80);

        //var pageRooms = new TabPage("Rooms"); pageRooms.Controls.Add(lvRooms);
        //var pagePlayers = new TabPage("Players"); pagePlayers.Controls.Add(lvPlayers);
        //tabs.TabPages.Add(pageRooms); tabs.TabPages.Add(pagePlayers);
        var scLeftTopBottom = new SplitContainer { Dock = DockStyle.Left, Width = 360, Orientation = Orientation.Horizontal, SplitterWidth = 6 };
        var scLeftMidBottom = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, SplitterWidth = 6 };

        // Top: 방 목록
        scLeftTopBottom.Panel1.Controls.Add(lvRooms);
        scLeftTopBottom.Panel1MinSize = 100;

        // Mid: 플레이어 목록
        scLeftMidBottom.Panel1.Controls.Add(lvPlayers);
        scLeftMidBottom.Panel1MinSize = 100;

        // 채팅 탭
        // Bottom: 채팅(로비/방/귓속말 탭은 그대로 유지)
        //var chatSplit = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal };
        //var chatTop = new TabControl { Dock = DockStyle.Fill };
        var chatTop = chatTabs; // 필드로 올려둔 chatTabs 사용
        var pLobby = new TabPage("로비"); pLobby.Controls.Add(lbLobby); pLobby.Controls.Add(tbLobby);
        var pRoom = new TabPage("방"); pRoom.Controls.Add(lbRoom); pRoom.Controls.Add(tbRoomChat);
        var pWhisp = new TabPage("귓속말");
        pWhisp.Controls.Add(lbWhisper);
        pWhisp.Controls.Add(tbWhisper);
        pWhisp.Controls.Add(tbWhisperTo);
        // [REMOVE] pWhisp.Controls.Add(tbWhisper);
        // [REMOVE] pWhisp.Controls.Add(tbWhisperTo);

        
        //var pNote = new TabPage("쪽지"); pNote.Controls.Add(lbNotes); pNote.Controls.Add(tbNote); pNote.Controls.Add(btnLoadNotes);
        chatTop.TabPages.AddRange(new[] { pLobby, pRoom, pWhisp });
        //chatSplit.Panel1.Controls.Add(chatTop);
        //tabChat.Controls.Add(chatSplit); // tabChat은 재사용
        //tabs.TabPages.Add(tabChat);

        status.Items.AddRange(new ToolStripItem[] { statL, statR });

        Controls.Add(canvas); Controls.Add(lvScore); //Controls.Add(tabs);


        scLeftMidBottom.Panel2.Controls.Add(chatTabs);
        scLeftTopBottom.Panel2.Controls.Add(scLeftMidBottom);
        Controls.Add(scLeftTopBottom);

        // ───────────────────────────────────────
        // [교체] 좌측 3단 영역을 항상 동일 높이로 맞추기 (재귀 방지)
        // ───────────────────────────────────────
        scLeftTopBottom.FixedPanel = FixedPanel.None;
        scLeftMidBottom.FixedPanel = FixedPanel.None;

        // 1/3 분할을 방해하지 않도록 MinSize 완화(필요시 조절)
        scLeftTopBottom.Panel1MinSize = Math.Max(scLeftTopBottom.Panel1MinSize, 60);
        scLeftMidBottom.Panel1MinSize = Math.Max(scLeftMidBottom.Panel1MinSize, 60);

        // 재진입 가드
        bool _syncing = false;

        void SetDistanceSafe(SplitContainer sc, int distance)
        {
            int H = sc.ClientSize.Height;
            int min = sc.Panel1MinSize;
            int max = Math.Max(min, H - sc.SplitterWidth - sc.Panel2MinSize);

            // 두 패널 MinSize 합이 창보다 크면 Panel1을 min으로 고정
            if (H < sc.Panel1MinSize + sc.SplitterWidth + sc.Panel2MinSize)
                distance = min;
            else
                distance = Math.Max(min, Math.Min(distance, max));

            if (distance != sc.SplitterDistance)
                sc.SplitterDistance = distance;
        }


        void SyncLeftSplits()
        {
            if (_syncing) return;
            _syncing = true;
            try
            {
                // 바깥: 1/3
                int H1 = scLeftTopBottom.ClientSize.Height;
                int topTarget = Math.Max(scLeftTopBottom.Panel1MinSize, H1 / 3);
                SetDistanceSafe(scLeftTopBottom, topTarget);

                // 안쪽: 자기 높이의 1/2
                int H2 = scLeftMidBottom.ClientSize.Height;
                int midTarget = Math.Max(scLeftMidBottom.Panel1MinSize, (H2 - scLeftMidBottom.SplitterWidth) / 2);
                SetDistanceSafe(scLeftMidBottom, midTarget);
            }
            finally { _syncing = false; }
        }


        // 폼 초기 표시/리사이즈/컨테이너 크기 변화 때 동기화
        this.Shown += (_, __) => SyncLeftSplits();
        this.Resize += (_, __) => SyncLeftSplits();
        scLeftTopBottom.SizeChanged += (_, __) => SyncLeftSplits();
        scLeftMidBottom.SizeChanged += (_, __) => SyncLeftSplits();

        // 사용자가 드래그해도 다시 1/3로 복원하고 싶으면 유지, 아니면 주석 처리
        scLeftTopBottom.SplitterMoved += (_, __) => { if (!_syncing) SyncLeftSplits(); };
        scLeftMidBottom.SplitterMoved += (_, __) => { if (!_syncing) SyncLeftSplits(); };
        // ───────────────────────────────────────

        Controls.Add(top); Controls.Add(tool); Controls.Add(status);


        canvas.Paint += OnCanvasPaint;
        KeyDown += OnKeyDown;

        // 방 더블클릭 → 즉시 입장 시도
        lvRooms.DoubleClick += async (_, __) => { if (_connected) await DoJoinRoomAsync(); };


        lvPlayers.DoubleClick += (_, __) =>
        {
            if (lvPlayers.SelectedItems.Count == 0) return;
            var name = lvPlayers.SelectedItems[0].Text.Replace("★", "").Replace("👑", "").Trim();
            chatTabs.SelectedIndex = 2;
            tbWhisperTo.Text = name; tbWhisper.Focus();
        };

        // ▶ 우클릭 시 커서 아래 항목 선택 + 대상 닉 저장
        lvPlayers.MouseDown += (s, e) =>
        {
            if (e.Button != MouseButtons.Right) return;
            var hit = lvPlayers.HitTest(e.Location);
            if (hit.Item != null)
            {
                hit.Item.Selected = true;
                lvPlayers.FocusedItem = hit.Item;
                _ctxTargetName = hit.Item.Text.Replace("★", "").Replace("👑", "").Trim();
            }
        };

        // 컨텍스트 메뉴 열릴 때/닫힐 때 갱신 잠깐 멈춤
        ctxPlayers.Opening += (_, __) => { _suspendPlayerListRefresh = true; };
        ctxPlayers.Closed += (_, __) => { _suspendPlayerListRefresh = false; UpdateSidebar(); };


        // 컨텍스트 메뉴
        ctxPlayers.Items.AddRange(new ToolStripItem[] { miInfo, miWhisper, new ToolStripSeparator(), miKick, miGiveHost });
        lvPlayers.ContextMenuStrip = ctxPlayers;
        miGiveHost.Enabled = _amIHost;

        // 선택된/우클릭된 항목의 닉 가져오기
        string? GetContextName()
        {
            string? name = _ctxTargetName;
            if (lvPlayers.SelectedItems.Count > 0)
                name = lvPlayers.SelectedItems[0].Text;
            if (string.IsNullOrWhiteSpace(name)) return null;
            return name.Replace("★", "").Replace("👑", "").Trim();
        }

        // 컨텍스트 메뉴가 뜨기 직전: 항목/권한/자기자신 여부로 버튼 활성화
        ctxPlayers.Opening += (s, e) =>
        {
            var name = GetContextName();
            bool hasSel = !string.IsNullOrWhiteSpace(name);
            bool isSelf = hasSel && name!.Equals(txtName.Text, StringComparison.OrdinalIgnoreCase);

            miInfo.Enabled = hasSel;               // 정보는 자기자신도 허용
            miWhisper.Enabled = hasSel && !isSelf;    // 자기자신에게 귓X
            miKick.Enabled = hasSel && !isSelf && _amIHost; // 자기자신 킥X
            miGiveHost.Enabled = hasSel && !isSelf && _amIHost; // 자기자신 위임X

            e.Cancel = !hasSel;
        };

        miInfo.Click += async (_, __) =>
        {
            var name = GetContextName();
            if (string.IsNullOrEmpty(name)) return;
            try
            {
                var info = await _conn.InvokeAsync<PlayerInfoDto>("GetPlayerInfo", name);
                MessageBox.Show(this,
                    $"{info.Name}\n레벨 {info.Level} (XP {info.Xp})\n전적 {info.Wins}승/{info.Losses}패",
                    "플레이어 정보");
            }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, "정보 조회 실패"); }
        };


        // 각 클릭 핸들러(자기자신 방지 가드 포함)
        miWhisper.Click += (_, __) =>
        {
            var name = GetContextName();
            if (string.IsNullOrEmpty(name) || name.Equals(txtName.Text, StringComparison.OrdinalIgnoreCase)) return;
            chatTabs.SelectedIndex = 2;
            tbWhisperTo.Text = name;
            tbWhisper.Focus();
        };

        miKick.Click += async (_, __) =>
        {
            try
            {
                var name = GetContextName();
                if (string.IsNullOrEmpty(name) || name.Equals(txtName.Text, StringComparison.OrdinalIgnoreCase)) return;
                if (string.IsNullOrEmpty(_currentRoomId)) return; // 방 밖에서 킥 금지

                // 서버 시그니처: (roomId, targetName, hostPlayerId)
                var ok = await _conn.InvokeAsync<bool>("KickPlayer", _currentRoomId, name, _playerId);
                if (!ok) MessageBox.Show(this, "강퇴 실패(권한/대상/상태 확인)");
            }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, "강퇴 실패"); }
        };

        miGiveHost.Click += async (_, __) =>
        {
            try
            {
                var name = GetContextName();
                if (string.IsNullOrEmpty(name) || name.Equals(txtName.Text, StringComparison.OrdinalIgnoreCase)) return;
                if (string.IsNullOrEmpty(_currentRoomId)) return;

                var ok = await _conn.InvokeAsync<bool>("DelegateHost", _currentRoomId, _playerId, name);
                if (!ok) MessageBox.Show(this, "위임 실패 (권한/대상 확인)");
            }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, "위임 실패"); }
        };



        btnHelp.Click += (_, __) => ShowHelp();
        btnRefreshRooms.Click += async (_, __) => await RefreshRoomsAsync();
        btnSolo.CheckedChanged += (_, __) => ToggleSolo(btnSolo.Checked);
        btnReady.CheckedChanged += async (_, __) => { if (_connected && !string.IsNullOrEmpty(_playerId)) await _conn.InvokeAsync("SetReady", _playerId, btnReady.Checked); };
        btnSkip.Click += async (_, __) =>
        {
            if (!_connected || string.IsNullOrEmpty(_playerId) || string.IsNullOrEmpty(_currentRoomId)) return;
            try
            {
                var ok = await _conn.InvokeAsync<bool>("SkipRemain", _currentRoomId, _playerId);
                if (!ok) MessageBox.Show(this, "스킵 실패(권한/상태 확인)");
            }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, "스킵 실패"); }
        };

        btnStart.Click += async (_, __) =>
        {
            if (!_connected || string.IsNullOrEmpty(_playerId)) return;
            try
            {
                var ok = await _conn.InvokeAsync<bool>("StartMatch", _playerId);
                if (!ok) MessageBox.Show(this, "시작 실패(방장/상태/레디 확인)");
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "시작 실패");
            }
        };


        btnCreateRoom.Click += async (_, __) =>
        {
            if (_uiBusy) return;

            using var f = new RoomCreateForm();
            if (f.ShowDialog(this) != DialogResult.OK) return;

            await RunUiBusy(async () =>
            {
                try
                {
                    var req = new CreateRoomRequest(
                        f.TitleText?.Trim() ?? "",
                        f.IsPrivate,
                        f.IsPrivate ? (f.Password ?? "") : null
                    );

                    // ❶ 방 예약(생성)
                    var createTask = _conn.InvokeAsync<CreateRoomResult>("CreateRoom", req);
                    var done = await Task.WhenAny(createTask, Task.Delay(5000));
                    if (done != createTask) { MessageBox.Show(this, "서버 응답 지연(생성)"); return; }
                    var res = await createTask;
                    MessageBox.Show(this, res.Message, res.Ok ? "OK" : "실패");
                    if (!res.Ok) return;

                    // ❷ 목록 새로고침(예약 방도 표시됨)
                    await RefreshRoomsAsync();

                    // ❸ 즉시 입장 시도: 타임아웃/비번 처리 포함
                    var joinTask = JoinSpecificRoomAsync(req.Title, req.IsPrivate ? req.Password : null);
                    done = await Task.WhenAny(joinTask, Task.Delay(5000));
                    if (done != joinTask) { MessageBox.Show(this, "서버 응답 지연(입장)"); return; }
                    await joinTask;

                    // 입장 성공 시 로비 명단 플래그 해제는 JoinSpecificRoomAsync 내부에서 수행
                }
                catch (Exception ex)
                {
                    MessageBox.Show(this, ex.Message, "방 생성/입장 실패");
                }
            });
        };

        btnConnect.Click += async (_, __) =>
        {
            using var lf = new LoginForm();
            if (lf.ShowDialog(this) != DialogResult.OK) return;
            txtUrl.Text = lf.FinalUrl;
            _loginId = lf.LoginId;
            _loginPassword = lf.Password;
            await ConnectAsync(_loginId, _loginPassword);
        };
        btnJoin.Click += async (_, __) => await DoJoinRoomAsync();

        // 나가기는 "연결유지/방만 나가기"
        btnLeave.Click += async (_, __) => await LeaveRoomOnlyAsync();

        btnNew.Click += (_, __) => { if (_solo is null) ToggleSolo(true); NewSoloGame(); };
        btnSave.Click += (_, __) => SaveSolo();
        btnLoad.Click += (_, __) => LoadSolo();
        
        btnShop.Click += (_, __) =>
        {
            using var f = new UnifiedShopForm(
                _conn?.State == HubConnectionState.Connected ? _conn : null,
                txtName.Text,
                () => _soloCoins,                          // 싱글 코인 getter
                coins => { _soloCoins = coins; SaveShopState(); UpdateSidebar(); }, // 싱글 코인 setter
                (itemId, price, isTheme) =>                // 싱글 구매 처리
                {
                    if (isTheme)
                    {
                        if (_ownedThemes.Contains(itemId) || _soloCoins < price) return false;
                        _ownedThemes.Add(itemId);
                    }
                    else
                    {
                        if (_ownedSkins.Contains(itemId) || _soloCoins < price) return false;
                        _ownedSkins.Add(itemId);
                    }
                    _soloCoins -= price;
                    SaveShopState();
                    FillCombos();
                    return true;
                }
            );
            f.ShowDialog(this);

            // 온라인/싱글 모두: 닫힌 뒤 최신 상태 반영
            _ = (_conn != null && _conn.State == HubConnectionState.Connected)
                ? RefreshOnlineInventoryAsync()
                : System.Threading.Tasks.Task.CompletedTask;

            UpdateSidebar();
        };

        // 닉 변경
        btnChangeNick.Click += async (_, __) =>
        {
            using var dlg = new Form { Text = "닉네임 변경", StartPosition = FormStartPosition.CenterParent, Width = 360, Height = 180, FormBorderStyle = FormBorderStyle.FixedDialog, MaximizeBox = false, MinimizeBox = false };
            var tbNick = new TextBox { Dock = DockStyle.Top, PlaceholderText = "새 닉네임" };
            var tbPwd = new TextBox { Dock = DockStyle.Top, PlaceholderText = "현재 비밀번호", UseSystemPasswordChar = true };
            var panel = new FlowLayoutPanel { Dock = DockStyle.Bottom, FlowDirection = FlowDirection.RightToLeft, Height = 40 };
            var ok = new Button { Text = "확인", DialogResult = DialogResult.OK, Width = 80 };
            var cancel = new Button { Text = "취소", DialogResult = DialogResult.Cancel, Width = 80 };
            panel.Controls.Add(ok); panel.Controls.Add(cancel);
            dlg.Controls.Add(panel); dlg.Controls.Add(tbPwd); dlg.Controls.Add(tbNick);
            if (dlg.ShowDialog(this) == DialogResult.OK)
            {
                try
                {
                    var res = await _conn.InvokeAsync<PurchaseResultDto>("ChangeNick", _loginId, tbPwd.Text, tbNick.Text);
                    MessageBox.Show(this, res.Message, res.Ok ? "OK" : "실패");
                    if (res.Ok)
                    {
                        txtName.Text = tbNick.Text.Trim();
                        await RefreshOnlineInventoryAsync();
                    }
                }
                catch (Exception ex) { MessageBox.Show(this, ex.Message, "닉변경 실패"); }
            }
        };

        // 채팅 입력
        tbLobby.KeyDown += async (_, e) => {
            if (e.KeyCode == Keys.Enter && _connected)
            {
                var t = tbLobby.Text.Trim();
                tbLobby.Clear();
                if (t.Length > 0) await _conn.InvokeAsync("SendLobbyChat", txtName.Text, t);
            }
        };

        // 귓속말 입력
        tbWhisper.KeyDown += async (_, e) =>
        {
            if (e.KeyCode == Keys.Enter && _connected)
            {
                var to = tbWhisperTo.Text.Trim();
                var t = tbWhisper.Text.Trim();
                tbWhisper.Clear();
                if (to.Length > 0 && t.Length > 0) await _conn.InvokeAsync("SendWhisper", txtName.Text, to, t);
            }
        };

        // 쪽지함
        //btnLoadNotes.Click += async (_, __) =>
        //{
        //    if (_connected)
        //    {
        //        var notes = await _conn.InvokeAsync<List<(DateTime, string, string)>>("GetNotes", txtName.Text);
        //        lbNotes.Items.Clear();
        //        foreach (var (utc, from, text) in notes) lbNotes.Items.Add($"[{utc:MM-dd HH:mm}] {from}: {text}");
        //    }
        //};

        // === 추가: 방 채팅 입력(엔터 → 서버 전송)
        tbRoomChat.KeyDown += async (_, e) =>
        {
            if (e.KeyCode != Keys.Enter) return;
            if (!_connected || string.IsNullOrEmpty(_currentRoomId)) return;

            var msg = tbRoomChat.Text.Trim();
            tbRoomChat.Clear();
            if (msg.Length == 0) return;

            // ⚠️ 서버 시그니처: SendRoomChat(string? roomId, string? from, string? text)
            await _conn.InvokeAsync("SendRoomChat", _currentRoomId, txtName.Text, msg);
        };

        // [MOD] 기존 한 줄짜리에서 로비 명단도 갱신
        _roomsTimer.Tick += async (_, __) =>
        {
            if (_connected)
            {
                await RefreshRoomsAsync();
                if (string.IsNullOrEmpty(_currentRoomId)) // 방 밖이면 로비 명단
                    await RefreshLobbyRosterAsync();
            }
        };

        _roomsTimer.Start();

        LoadShopState();

        // 콤보 선택 변경 → 적용
        cmbSkin.SelectedIndexChanged += async (_, __) => { if (_fillingCombos) return; await ApplySkinSelectionAsync(); };
        cmbTheme.SelectedIndexChanged += async (_, __) => { if (_fillingCombos) return; await ApplyThemeSelectionAsync(); };

        FillCombos();
    }

    void ShowHelp()
    {
        MessageBox.Show(
            "조작: ⬆⬇⬅➡, 일시정지: ESC\n" +
            "싱글: 사과=코인2, 점수5마다 레벨업(속도↑), 드물게 골드/경치 드랍, 방향 연타=가속\n" +
            "멀티: 로그인→방 목록 선택→[입장], 비공개 방은 비번 입력, 강퇴/귓/쪽지/상점/닉변경 지원",
            "게임 방법");
    }
    // 허브 연결(+ 로그인 검증)
    async Task ConnectAsync(string loginId, string password)
    {
        try
        {
            // === [추가] 이전 연결/핸들러 정리: 중복 수신 방지 ===
            if (_conn is not null)
            {
                try
                {
                    _conn.Remove("Snapshot");
                    _conn.Remove("MatchFinished");
                    _conn.Remove("LobbyChat");
                    _conn.Remove("RoomChat");
                    _conn.Remove("RoomSystem");
                    _conn.Remove("RoomRoster");
                    _conn.Remove("Whisper");
                    _conn.Remove("ForceDisconnected");
                }
                catch { /* 무시 */ }

                try { await _conn.DisposeAsync(); } catch { /* 무시 */ }
                _conn = null!;
            }
            // ===================================================

            ToggleSolo(false);
            btnConnect.Enabled = false; spin.Visible = spinLabel.Visible = true;

            _conn = new HubConnectionBuilder().WithUrl(txtUrl.Text).WithAutomaticReconnect().Build();

            _conn.Reconnecting += error =>
            {
                this.BeginInvoke(() =>
                {
                    statL.Text = "Reconnecting...";
                    btnJoin.Enabled = false;
                    btnReady.Enabled = false;
                    btnStart.Enabled = false;
                });
                return Task.CompletedTask;
            };
            _conn.Reconnected += id =>
            {
                this.BeginInvoke(async () =>
                {
                    statL.Text = "Online (reconnected)";
                    btnJoin.Enabled = true;
                    await RefreshRoomsAsync();
                });
                return Task.CompletedTask;
            };
            _conn.Closed += ex =>
            {
                this.BeginInvoke(() =>
                {
                    statL.Text = "Disconnected";
                    btnJoin.Enabled = false;
                    btnReady.Enabled = false;
                    btnStart.Enabled = false;
                });
                return Task.CompletedTask;
            };

            // ★ 싱글 중에는 서버 스냅샷 무시 (깜빡임 방지)
            _conn.On<GameSnapshot>("Snapshot", s =>
            {
                if (_solo != null) return;
                this.BeginInvoke(new Action(() =>
                {
                    _snap = s;
                    UpdateSidebar();
                }));
            });


            _conn.On<MatchFinishedDto>("MatchFinished", r => this.BeginInvoke(new Action(() => ShowRewards(r))));


            _conn.On<DateTime, string, string>("LobbyChat", (utc, from, text) =>
                this.BeginInvoke(() =>
                {
                    lbLobby.Items.Add($"[{utc:HH:mm:ss}] {from}: {text}");
                    lbLobby.TopIndex = lbLobby.Items.Count - 1;
                })
            );

            // === RoomChat(신규 페이로드) / RoomSystem
            // 강타입 핸들러
            _conn.On<RoomChatDto>("RoomChat", dto =>
            {
                this.BeginInvoke(new Action(() =>
                {
                    lbRoom.Items.Add($"[{dto.At}] ({dto.RoomId}) {dto.Sender}: {dto.Text}");
                    lbRoom.TopIndex = lbRoom.Items.Count - 1;
                }));
            });


            _conn.On<string>("RoomSystem", msg =>
            {
                this.BeginInvoke(new Action(() =>
                {
                    lbRoom.Items.Add($"[SYSTEM] {msg}");
                    lbRoom.TopIndex = lbRoom.Items.Count - 1;
                }));
            });

            // === RoomRoster(호스트/명단 정보)
            _conn.On<RosterItem[]>("RoomRoster", arr =>
            {
                try
                {
                    var hostPid = arr.FirstOrDefault(it => it.IsHost).PlayerId;
                    var amIHost = !string.IsNullOrEmpty(_playerId) && hostPid == _playerId;

                    _hostPlayerId = hostPid;
                    _amIHost = amIHost;

                    this.BeginInvoke(new Action(() =>
                    {
                        miKick.Enabled = _amIHost;
                        miGiveHost.Enabled = _amIHost;
                        btnStart.Enabled = _amIHost;
                        btnSkip.Enabled = _amIHost;
                        UpdateSidebar(); // 👑표시/버튼 반영
                    }));
                }
                catch { /* 무시 */ }
            });


            _conn.On<DateTime, string, string, string>("Whisper", (utc, from, to, text) => this.BeginInvoke(() =>
            {
                if (to.Equals(txtName.Text, StringComparison.OrdinalIgnoreCase) || from.Equals(txtName.Text, StringComparison.OrdinalIgnoreCase))
                    lbWhisper.Items.Add($"[{utc:HH:mm:ss}] {from}→{to}: {text}");
            }));

            _conn.On<string>("ForceDisconnected", (reason) =>
            {
                _ = this.BeginInvoke(new Action(async () =>
                {
                            // 강퇴 키워드만 포함돼도 로비로 이동(연결 유지)
                    if (!string.IsNullOrEmpty(reason) &&
                        reason.IndexOf("kick", StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        MessageBox.Show(this, $"방에서 강퇴되었습니다. 로비로 이동합니다. ({reason})");
                        await LeaveRoomOnlyAsync();   // ✅ 전체 종료(X)
                        return;
                    }

                    // 그 외 사유는 진짜 접속 해제
                    MessageBox.Show(this, $"접속이 해제되었습니다. ({reason})");
                    await LeaveAsync();               // ✅ 전체 종료(O)
                }));
            });

            _conn.On<string>("KickedFromRoom", roomId =>
            {
                this.BeginInvoke(new Action(async () =>
                {
                    MessageBox.Show(this, "방에서 강퇴되었습니다. 로비로 이동합니다.");
                    await LeaveRoomOnlyAsync();
                }));
            });


            await _conn.StartAsync();

            try
            {
                var ok = await _conn.InvokeAsync<bool>("Ping");
                if (!ok) throw new Exception("Hub ping failed");
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, $"허브 연결 확인 실패: {ex.Message}");
                return;
            }

            // 로그인
            var login = await _conn.InvokeAsync<LoginResultDto>("Login", loginId, password);
            txtName.Text = login.Nick;
            var prof = login.Profile;

            statL.Text = $"Online (logged in)  {login.Nick}  Lv.{prof.Level}  XP:{prof.Xp}  Coins:{prof.Coins}";
            await RefreshOnlineInventoryAsync();

            // 방에 아직 안 들어가 있으면 접속자 목록 즉시 표시
            await RefreshLobbyRosterAsync();

            btnLeave.Enabled = true;
            btnJoin.Enabled = true;
            btnCreateRoom.Enabled = true;
            btnReady.Enabled = false;
            btnStart.Enabled = false;

            await RefreshRoomsAsync();
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "Connect failed");
        }
        finally
        {
            btnConnect.Enabled = true; spin.Visible = spinLabel.Visible = false;
        }
    }

    // 방 입장(목록에서 선택)
    async Task DoJoinRoomAsync()
    {
        if (_uiBusy || _isJoining) return;
        _isJoining = true;
        btnJoin.Enabled = false;

        try
        {
            if (!_connected) { MessageBox.Show("먼저 접속(로그인)하세요."); return; }
            if (lvRooms.SelectedItems.Count == 0) { MessageBox.Show("입장할 방을 목록에서 선택하세요."); return; }

            var roomId = lvRooms.SelectedItems[0].Text;

            // 이미 그 방에 있으면 차단
            if (!string.IsNullOrEmpty(_playerId) &&
                string.Equals(_currentRoomId, roomId, StringComparison.OrdinalIgnoreCase))
            {
                MessageBox.Show("이미 해당 방에 입장 중입니다.");
                return;
            }

            // 싱글 중이면 먼저 끄기(깜빡임 방지)
            if (_solo != null) ToggleSolo(false);

            // 다른 방에 들어가 있었다면 우선 방만 나가기
            if (!string.IsNullOrEmpty(_playerId))
                await LeaveRoomOnlyAsync();

            await JoinSpecificRoomAsync(roomId, null);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "JoinRoom failed");
        }
        finally
        {
            _isJoining = false;
            btnJoin.Enabled = true;
        }
    }

    // 생성 직후 자동입장/목록에서 입장 공용 헬퍼
    async Task JoinSpecificRoomAsync(string roomId, string? password)
    {
        try
        {
            // 혹시 모를 싱글 잔여 상태 제거
            if (_solo != null) ToggleSolo(false);

            // 이미 같은 방이면 무의미
            if (!string.IsNullOrEmpty(_currentRoomId) &&
                string.Equals(_currentRoomId, roomId, StringComparison.OrdinalIgnoreCase))
                return;

            // 비공개면 비번 입력
            if (password == null)
            {
                var isPrivate = await _conn.InvokeAsync<bool>("IsPrivateRoom", roomId);
                if (isPrivate)
                {
                    using var pwdDlg = new Form { Text = "비공개 방 비밀번호", StartPosition = FormStartPosition.CenterParent, Width = 340, Height = 140, FormBorderStyle = FormBorderStyle.FixedDialog, MaximizeBox = false, MinimizeBox = false };
                    var tbPwd = new TextBox { Dock = DockStyle.Top, PlaceholderText = "비밀번호", UseSystemPasswordChar = true };
                    var panel = new FlowLayoutPanel { Dock = DockStyle.Bottom, FlowDirection = FlowDirection.RightToLeft, Height = 40 };
                    var ok = new Button { Text = "입장", DialogResult = DialogResult.OK, Width = 80 };
                    var cancel = new Button { Text = "취소", DialogResult = DialogResult.Cancel, Width = 80 };
                    panel.Controls.Add(ok); panel.Controls.Add(cancel);
                    pwdDlg.Controls.Add(panel); pwdDlg.Controls.Add(tbPwd);
                    if (pwdDlg.ShowDialog(this) != DialogResult.OK) return;
                    password = tbPwd.Text ?? "";
                }
            }

            await RefreshOnlineInventoryAsync();
            var selectedCosmeticId = _onlineSelectedSkinId ?? (cmbSkin.SelectedItem as ComboItem)?.Id ?? _selectedSkinId;

            var acc = await _conn.InvokeAsync<JoinAccepted>("JoinRoom",
                new JoinRequest(txtName.Text, roomId, selectedCosmeticId, password));

            _playerId = acc.PlayerId;
            _currentRoomId = roomId;
            Text = $"Snake - {txtName.Text}@{roomId}";
            statL.Text = $"In-Room  Lv.{acc.AccountLevel}  XP:{acc.AccountXp}  Coins:{_onlineCoins}";

            btnReady.Enabled = true;

            _showingLobbyRoster = false;


            // 방장 여부 확인 → 즉시 버튼/메뉴 반영 (정확한 호스트ID는 RoomRoster에서 재확정)
            var isHost = await _conn.InvokeAsync<bool>("AmIHost", _playerId);
            _amIHost = isHost;
            btnStart.Enabled = isHost;
            miKick.Enabled = isHost;
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "JoinRoom failed");
        }
    }

    async Task RefreshOnlineInventoryAsync()
    {
        var prof = await _conn.InvokeAsync<AccountProfileDto>("GetAccountProfile", txtName.Text);
        var inv = await _conn.InvokeAsync<InventoryDto>("GetInventory", txtName.Text);

        _onlineCoins = inv.Coins;
        _onlineSkins = new HashSet<string>(inv.OwnedCosmetics ?? new(), StringComparer.OrdinalIgnoreCase);
        _onlineThemes = new HashSet<string>(inv.OwnedThemes ?? new(), StringComparer.OrdinalIgnoreCase);
        _onlineSelectedSkinId = inv.SelectedCosmeticId ?? prof.SelectedCosmeticId;
        _onlineSelectedThemeId = inv.SelectedThemeId ?? prof.SelectedThemeId;

        FillCombos();
        statL.Text = $"Online  {txtName.Text}  Lv.{prof.Level}  XP:{prof.Xp}  Coins:{prof.Coins}";
    }

    // 허브 완전 종료(로그아웃)
    async Task LeaveAsync()
    {
        try { if (_conn is not null) await _conn.DisposeAsync(); }
        finally
        {
            _playerId = ""; _currentRoomId = null; _snap = null; UpdateSidebar();
            statL.Text = "Disconnected";
            btnLeave.Enabled = false;
            btnJoin.Enabled = false;
            btnCreateRoom.Enabled = false;
            btnReady.Enabled = false; btnReady.Checked = false;
            btnStart.Enabled = false;
            _amIHost = false; _hostPlayerId = null;
        }
    }

    // 방만 나가기(허브 연결 유지)
    async Task LeaveRoomOnlyAsync()
    {
        try
        {
            if (_conn is not null && _conn.State == HubConnectionState.Connected)
            {
                await _conn.InvokeAsync("LeaveByConnection", "client-leave");
            }
        }
        catch { /* 서버가 이미 정리했을 수도 있음 */ }
        finally
        {
            _playerId = ""; _currentRoomId = null; _snap = null; UpdateSidebar();

            // 로그인 상태 유지
            statL.Text = $"Online (logged in)  {txtName.Text}";
            btnReady.Checked = false; btnReady.Enabled = false;
            btnStart.Enabled = false;
            btnJoin.Enabled = true;
            btnCreateRoom.Enabled = true;
            _amIHost = false; _hostPlayerId = null;

            //로비로 돌아왔으니 로비 명단 표시
            _showingLobbyRoster = true;
            await RefreshLobbyRosterAsync();
        }
    }

    void ToggleSolo(bool on)
    {
        if (on)
        {
            // 연결은 유지하고 방만 나가기
            if (_connected && !string.IsNullOrEmpty(_playerId))
                _ = LeaveRoomOnlyAsync();

            NewSoloGame();
            _soloTick.Start();
            statL.Text = $"Solo  Lv.{_lastSoloLevel}  Coins:{_soloCoins}";
        }
        else
        {
            _soloTick.Stop(); _solo = null; _snap = null; UpdateSidebar();
        }
    }

    void NewSoloGame()
    {
        _worldW = 48; _worldH = 32;

        var chosen = CosmeticCatalog.InstanceOf(_selectedSkinId);
        _solo = new SoloEngine(_worldW, _worldH, txtName.Text, chosen);
        _lastSoloScore = _solo.Score;
        _lastSoloLevel = _solo.Level;
        _soloDeathShown = false;
        _paused = false;
        _floaters.Clear();
        _snap = _solo.Current;
        _tapStreak = 0; _solo.SetBoost(0);
        ApplySoloSpeed();
        UpdateSidebar();
    }

    void ApplySoloSpeed()
    {
        if (_solo is null) return;
        _soloTick.Interval = _solo.RecommendedIntervalMs;
        statL.Text = $"Solo{(_paused ? " (일시정지)" : "")}  Lv.{_solo.Level}  Coins:{_soloCoins}  Speed:{1000 / _soloTick.Interval}/s";
    }

    void ShowSoloDeathPopup()
    {
        if (_solo is null) return;
        var r = MessageBox.Show(this,
            $"사망했습니다!\n\n점수: {_solo.Score}  레벨: {_solo.Level}\n\n" +
            "[예]=새로하기   [아니오]=불러오기   [취소]=닫기",
            "게임 오버", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Information);

        if (r == DialogResult.Yes) NewSoloGame();
        else if (r == DialogResult.No) LoadSolo();
    }

    void SaveSolo()
    {
        if (_solo is null) { MessageBox.Show("싱글플레이를 시작하세요."); return; }
        using var dlg = new SaveFileDialog { Filter = "Snake Save (*.snk)|*.snk|JSON (*.json)|*.json|All files|*.*", FileName = "solo.snk" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        var state = _solo.ExportState();
        var json = JsonSerializer.Serialize(state, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(dlg.FileName, json);
        MessageBox.Show("저장 완료!");
    }

    void LoadSolo()
    {
        using var dlg = new OpenFileDialog { Filter = "Snake Save (*.snk;*.json)|*.snk;*.json|All files|*.*" };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;

        var json = File.ReadAllText(dlg.FileName);
        var state = JsonSerializer.Deserialize<SoloSaveData>(json);
        if (state is null) { MessageBox.Show("불러오기에 실패했습니다."); return; }

        _worldW = state.W <= 0 ? 48 : state.W;
        _worldH = state.H <= 0 ? 32 : state.H;

        var chosen = CosmeticCatalog.InstanceOf(_selectedSkinId);
        _solo = new SoloEngine(state.W, state.H, state.Name, chosen);
        _solo.ImportState(state);
        _soloDeathShown = !_solo.Alive;
        _lastSoloScore = _solo.Score;
        _lastSoloLevel = _solo.Level;
        _paused = false;
        _floaters.Clear();
        _snap = _solo.Current;
        _tapStreak = 0; _solo.SetBoost(0);
        ApplySoloSpeed();
        if (!_soloTick.Enabled) _soloTick.Start();
        UpdateSidebar();
        MessageBox.Show("불러오기 완료!");
    }

    static void TryPlay(SoundPlayer sp) { try { sp.Play(); } catch { } }

    async Task RefreshRoomsAsync()
    {
        try
        {
            if (_conn is null || _conn.State == HubConnectionState.Disconnected)
            {
                lvRooms.Items.Clear();
                return;
            }
            var rooms = await _conn.InvokeAsync<List<RoomBrief>>("ListRooms");
            lvRooms.BeginUpdate();
            lvRooms.Items.Clear();
            foreach (var r in rooms)
            {
                var it = new ListViewItem(r.RoomId);
                it.SubItems.Add(r.Players.ToString());
                it.SubItems.Add(r.Ready.ToString());
                it.SubItems.Add(r.Phase.ToString());
                lvRooms.Items.Add(it);
            }
            lvRooms.EndUpdate();
        }
        catch { }
    }

    // 방 밖(로비)일 때 플레이어 탭에 온라인 유저/방 여부 표시
    async Task RefreshLobbyRosterAsync()
    {
        if (_conn is null || _conn.State != HubConnectionState.Connected) return;
        if (!string.IsNullOrEmpty(_currentRoomId)) return; // 방 안에 있을 땐 스냅샷으로 표현

        try
        {
            // 서버에 구현 필요: ListOnlinePlayers() -> List<OnlinePlayerBrief>
            var list = await _conn.InvokeAsync<List<OnlinePlayerBrief>>("ListOnlinePlayers");

            lvPlayers.BeginUpdate();
            lvPlayers.Items.Clear();
            foreach (var p in list.OrderByDescending(x => x.Level).ThenBy(x => x.Name))
            {
                var prefixMine = p.Name.Equals(txtName.Text, StringComparison.OrdinalIgnoreCase) ? "★ " : "";
                var prefixHost = p.IsHost ? "👑 " : "";
                var it = new ListViewItem($"{prefixMine}{prefixHost}{p.Name}");
                it.SubItems.Add(p.Level.ToString());
                it.SubItems.Add(p.InRoom ? $"방:{p.RoomId}" : "로비");
                lvPlayers.Items.Add(it);
            }
            lvPlayers.EndUpdate();

            _showingLobbyRoster = true;
            statR.Text = $"온라인 {list.Count}명";
        }
        catch
        {
            // 허브 메서드가 아직 없으면 조용히 무시
        }
    }



    void UpdateSidebar()
    {
        if (InvokeRequired)
        {
            BeginInvoke(new Action(UpdateSidebar));
            return;
        }
        // 컨텍스트 메뉴 열려 있으면 리빌드 생략
        if (_suspendPlayerListRefresh)
        {
            if (_snap != null)
            {
                var coinText = (_solo != null) ? $"Coins:{_soloCoins}" :
                               (_connected ? $"Coins:{_onlineCoins}" : $"Coins:{_soloCoins}");
                statR.Text = $"{_snap.Phase}  Tick:{_snap.ServerTick}  Remain:{_snap.PhaseTicksRemaining}  {coinText}";
            }
            else if (_showingLobbyRoster)
            {
                // 로비 명단을 유지
            }
            else statR.Text = "";
            canvas.Invalidate();
            return;
        }

        // [ADD] 스냅샷이 없고 로비 명단을 보여주는 중이면 건드리지 않음
        if (_snap == null && _showingLobbyRoster)
        {
            canvas.Invalidate();
            return;
        }

        lvPlayers.BeginUpdate();
        lvPlayers.Items.Clear();
        if (_snap != null)
        {
            foreach (var p in _snap.Players.OrderByDescending(x => x.Score))
            {
                var prefixMine = (p.PlayerId == _playerId) ? "★ " : "";
                var prefixHost = (!string.IsNullOrEmpty(_hostPlayerId) && p.PlayerId == _hostPlayerId) ? "👑 " : "";
                var it = new ListViewItem($"{prefixMine}{prefixHost}{p.Name}");
                it.SubItems.Add(p.Level.ToString());
                it.SubItems.Add("방 내부"); // [ADD] 스냅샷 모드일 땐 상태 고정
                lvPlayers.Items.Add(it);
            }

            var coinText = (_solo != null) ? $"Coins:{_soloCoins}" :
                           (_connected ? $"Coins:{_onlineCoins}" : $"Coins:{_soloCoins}");
            statR.Text = $"{_snap.Phase}  Tick:{_snap.ServerTick}  Remain:{_snap.PhaseTicksRemaining}  {coinText}";
        }
        else
        {
            statR.Text = "";
        }
        lvPlayers.EndUpdate();
        canvas.Invalidate();
    }



    void OnCanvasPaint(object? s, PaintEventArgs e)
    {
        // ★ 싱글 중이면 로컬 테마 강제 사용
        var themeId = (_solo != null) ? _selectedThemeId
            : (_connected ? (_onlineSelectedThemeId ?? "theme_dark") : _selectedThemeId);

        var theme = ThemeCatalog.ThemeOf(themeId);
        var g = e.Graphics;

        g.Clear(Hex(theme.BgHex));

        // === 월드 영역 계산: 가운데 정렬 (상단 32px HUD 제외) ===
        int worldPxW = _worldW * _cell;
        int worldPxH = _worldH * _cell;

        _originX = Math.Max(0, (canvas.Width - worldPxW) / 2);
        _originY = 32 + Math.Max(0, ((canvas.Height - 32) - worldPxH) / 2);

        // 경계선/그리드
        using var gridPen = new Pen(Hex(theme.GridHex), 1);
        for (int x = 0; x <= worldPxW; x += _cell)
            g.DrawLine(gridPen, _originX + x, _originY, _originX + x, _originY + worldPxH);
        for (int y = 0; y <= worldPxH; y += _cell)
            g.DrawLine(gridPen, _originX, _originY + y, _originX + worldPxW, _originY + y);

        // 외곽 테두리
        using (var borderPen = new Pen(Hex(theme.TextHex), 2))
            g.DrawRectangle(borderPen, new Rectangle(_originX, _originY, worldPxW, worldPxH));

        using var textB = new SolidBrush(Hex(theme.TextHex));
        g.DrawString($"{(_snap?.Phase.ToString() ?? "Idle")} - Tick:{_snap?.ServerTick ?? 0}  Remain:{_snap?.PhaseTicksRemaining ?? 0}",
                     Font, textB, 4, 4);

        if (_snap == null) { g.DrawString("접속하거나 [싱글플레이]를 켜세요.", Font, textB, 10, 10); return; }

        var appleRect = Cell(_snap.Apple);
        using (var appleB = new SolidBrush(Hex(theme.AppleHex))) g.FillEllipse(appleB, appleRect);

        foreach (var d in _snap.Loot)
        {
            var r = Cell(d.Pos);
            var color = d.Kind == LootKind.Gold ? Hex(theme.GoldHex) : Hex(theme.XpHex);
            using var b = new SolidBrush(color);
            g.FillEllipse(b, r);
            using var numBrush = new SolidBrush(Color.Black);
            using var f = new Font(Font, FontStyle.Bold);
            var txt = d.Amount.ToString();
            var sz = g.MeasureString(txt, f);
            g.DrawString(txt, f, numBrush, r.Left + (r.Width - sz.Width) / 2, r.Top + (r.Height - sz.Height) / 2 - 1);
        }

        foreach (var p in _snap.Players)
        {
            var isDead = !p.Alive;
            for (int i = 0; i < p.Body.Count; i++)
            {
                var r = Cell(p.Body[i]);
                Rendering.SkinPainter.PaintCell(g, r, p.Cosmetic.Skin, head: i == 0);
                if (isDead) { using var overlay = new SolidBrush(Color.FromArgb(140, 0, 0, 0)); g.FillRectangle(overlay, r); }
                if (i == 0)
                {
                    if (!isDead)
                    {
                        var label = $"{p.Name}  Lv.{p.Level}";
                        g.DrawString(label, Font, textB, r.Left, r.Top - 16);
                    }
                    if (p.PlayerId == _playerId) g.DrawRectangle(Pens.White, r);
                }
            }
        }

        foreach (var fl in _floaters)
        {
            var r = Cell(fl.Grid);
            var y = r.Top - 14 - fl.Age;
            using var brush = new SolidBrush(fl.Kind == LootKind.Gold ? Hex(theme.GoldHex) : Hex(theme.XpHex));
            using var f = new Font(Font, FontStyle.Bold);
            g.DrawString(fl.Text, f, brush, r.Left, y);
        }

        if (_paused)
        {
            using var blk = new SolidBrush(Color.FromArgb(120, 0, 0, 0));
            g.FillRectangle(blk, new Rectangle(0, 0, canvas.Width, canvas.Height));
            using var f = new Font(Font.FontFamily, 24, FontStyle.Bold);
            var pause = "PAUSED";
            var sz = g.MeasureString(pause, f);
            using var br = new SolidBrush(Color.White);
            g.DrawString(pause, f, br, (canvas.Width - sz.Width) / 2, (canvas.Height - sz.Height) / 2);
        }
    }

    // 월드 좌표 → 화면 좌표
    Rectangle Cell(Point pt) => new(_originX + pt.X * _cell, _originY + pt.Y * _cell, _cell, _cell);

    async void OnKeyDown(object? sender, KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Escape)
        {
            if (_solo is not null)
            {
                _paused = !_paused; ApplySoloSpeed(); canvas.Invalidate(); e.Handled = true; return;
            }
        }
        var key = e.KeyCode switch
        {
            Keys.Up => InputKey.Up,
            Keys.Down => InputKey.Down,
            Keys.Left => InputKey.Left,
            Keys.Right => InputKey.Right,
            _ => (InputKey)(-1)
        };
        if ((int)key < 0) return;

        if (_solo is not null)
        {
            if (!_paused)
            {
                BumpTapBoost(key);
                _solo.Apply(key);
            }
            e.Handled = true;
            return;
        }
        if (string.IsNullOrEmpty(_playerId) || !_connected) return;

        await _conn.InvokeAsync("SendInput",
            new InputCommand(_playerId, Environment.TickCount, key, DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()));
        e.Handled = true;
    }

    protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
    {
        if (TryHandleArrow(keyData)) return true;
        return base.ProcessCmdKey(ref msg, keyData);
    }
    private bool TryHandleArrow(Keys keyData)
    {
        InputKey? key = keyData switch
        {
            Keys.Up => InputKey.Up,
            Keys.Down => InputKey.Down,
            Keys.Left => InputKey.Left,
            Keys.Right => InputKey.Right,
            _ => (InputKey?)null
        };
        if (key is null) return false;
        if (_solo is not null)
        {
            if (!_paused)
            {
                BumpTapBoost(key.Value);
                _solo.Apply(key.Value);
            }
            return true;
        }
        if (!string.IsNullOrEmpty(_playerId) && _connected)
        {
            _ = _conn.InvokeAsync("SendInput",
                new InputCommand(_playerId, Environment.TickCount, key.Value, DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()));
            return true;
        }
        return false;
    }

    void BumpTapBoost(InputKey key)
    {
        var now = Environment.TickCount64;
        if (_lastTapKey == key && now - _lastTapAtMs <= TAP_WINDOW_MS) _tapStreak++;
        else _tapStreak = 1;
        _lastTapKey = key; _lastTapAtMs = now;
        _solo?.SetBoost(Math.Min(Math.Max(_tapStreak - 1, 0), 5)); // 0~5단계
        ApplySoloSpeed();
    }

    void ShowRewards(MatchFinishedDto r)
    {
        var text = string.Join(Environment.NewLine, r.Rewards.OrderBy(x => x.Rank)
            .Select(x => $"{x.Rank}. {x.Name}  +{x.GainedXp}XP  +{x.GainedCoins}c {(x.NewCosmeticUnlocked ? $"UNLOCK:{x.UnlockedCosmeticId}" : "")}"));
        MessageBox.Show(this, $"Match Finished!\n\n{text}\n\nYour Lv:{r.YourLevel} XP:{r.YourXp}", "Results");
        if (_connected) _ = RefreshOnlineInventoryAsync();
    }

    static Color Hex(string hex)
    {
        var s = hex.StartsWith('#') ? hex[1..] : hex;
        byte a = 0xFF, r = 0, g = 0, b = 0;
        if (s.Length == 6) { r = Convert.ToByte(s[..2], 16); g = Convert.ToByte(s.Substring(2, 2), 16); b = Convert.ToByte(s.Substring(4, 2), 16); }
        else if (s.Length == 8) { a = Convert.ToByte(s[..2], 16); r = Convert.ToByte(s.Substring(2, 2), 16); g = Convert.ToByte(s.Substring(4, 2), 16); b = Convert.ToByte(s.Substring(6, 2), 16); }
        return Color.FromArgb(a, r, g, b);
    }

    void FillCombos()
    {
        _fillingCombos = true;
        try
        {
            cmbSkin.Items.Clear();
            cmbTheme.Items.Clear();

            if (_connected)
            {
                foreach (var s in CosmeticCatalog.AllSkins.Where(x => _onlineSkins.Contains(x.Id)))
                    cmbSkin.Items.Add(new ComboItem(s.Id, $"[M] {s.Display}"));

                foreach (var th in ThemeCatalog.AllThemes.Where(x => _onlineThemes.Contains(x.Id)))
                    cmbTheme.Items.Add(new ComboItem(th.Id, $"[M] {th.Display}"));

                if (cmbSkin.Items.Count > 0)
                {
                    var sel = _onlineSelectedSkinId ?? _selectedSkinId;
                    var ix = FindIndex(cmbSkin, sel);
                    cmbSkin.SelectedIndex = ix >= 0 ? ix : 0;
                }
                if (cmbTheme.Items.Count > 0)
                {
                    var sel = _onlineSelectedThemeId ?? _selectedThemeId;
                    var ix = FindIndex(cmbTheme, sel);
                    cmbTheme.SelectedIndex = ix >= 0 ? ix : 0;
                }
            }
            else
            {
                foreach (var s in CosmeticCatalog.AllSkins.Where(x => _ownedSkins.Contains(x.Id)))
                    cmbSkin.Items.Add(new ComboItem(s.Id, s.Display));

                foreach (var th in ThemeCatalog.AllThemes.Where(x => _ownedThemes.Contains(x.Id)))
                    cmbTheme.Items.Add(new ComboItem(th.Id, th.Display));

                if (cmbSkin.Items.Count > 0)
                {
                    var ix = FindIndex(cmbSkin, _selectedSkinId);
                    cmbSkin.SelectedIndex = ix >= 0 ? ix : 0;
                }
                if (cmbTheme.Items.Count > 0)
                {
                    var ix = FindIndex(cmbTheme, _selectedThemeId);
                    cmbTheme.SelectedIndex = ix >= 0 ? ix : 0;
                }
            }
        }
        finally
        {
            _fillingCombos = false;
        }
    }

    static int FindIndex(ComboBox cb, string? id)
    {
        if (id is null) return -1;
        for (int i = 0; i < cb.Items.Count; i++) if (cb.Items[i] is ComboItem it && it.Id == id) return i;
        return -1;
    }

    void LoadShopState()
    {
        try
        {
            if (!File.Exists(SHOP_STATE_FILE)) return;
            var s = JsonSerializer.Deserialize<ShopState>(File.ReadAllText(SHOP_STATE_FILE));
            if (s is null) return;
            _soloCoins = s.Coins;
            _ownedSkins.Clear(); foreach (var id in s.OwnedSkins ?? Enumerable.Empty<string>()) _ownedSkins.Add(id);
            _ownedThemes.Clear(); foreach (var id in s.OwnedThemes ?? Enumerable.Empty<string>()) _ownedThemes.Add(id);
            if (!string.IsNullOrWhiteSpace(s.SelectedSkinId)) _selectedSkinId = s.SelectedSkinId!;
            if (!string.IsNullOrWhiteSpace(s.SelectedThemeId)) _selectedThemeId = s.SelectedThemeId!;
        }
        catch { }
    }
    void SaveShopState()
    {
        var s = new ShopState
        {
            Coins = _soloCoins,
            OwnedSkins = _ownedSkins.ToList(),
            OwnedThemes = _ownedThemes.ToList(),
            SelectedSkinId = _selectedSkinId,
            SelectedThemeId = _selectedThemeId
        };
        File.WriteAllText(SHOP_STATE_FILE, JsonSerializer.Serialize(s, new JsonSerializerOptions { WriteIndented = true }));
    }

    void OpenLocalShop()
    {
        using var f = new LocalShopForm(
            () => _soloCoins,
            coins => { _soloCoins = coins; SaveShopState(); UpdateSidebar(); },
            (itemId, price, isTheme) =>
            {
                if (isTheme)
                {
                    if (_ownedThemes.Contains(itemId) || _soloCoins < price) return false;
                    _ownedThemes.Add(itemId);
                }
                else
                {
                    if (_ownedSkins.Contains(itemId) || _soloCoins < price) return false;
                    _ownedSkins.Add(itemId);
                }
                _soloCoins -= price;
                SaveShopState();
                FillCombos();
                return true;
            }
        );
        f.ShowDialog(this);
        UpdateSidebar();
    }

    async void OpenOnlineShop()
    {
        using var f = new OnlineShopForm(_conn, txtName.Text);
        f.ShowDialog(this);
        await RefreshOnlineInventoryAsync();
    }

    // 선택 적용(온라인/로컬)
    async Task ApplySkinSelectionAsync()
    {
        var id = (cmbSkin.SelectedItem as ComboItem)?.Id;
        if (string.IsNullOrEmpty(id)) return;

        if (_connected)
        {
            try
            {
                var ok = await _conn.InvokeAsync<bool>(
                    "SetSelection",
                    txtName.Text,
                    new SetSelectionRequest(CosmeticId: id, ThemeId: null)
                );
                if (!ok) { MessageBox.Show(this, "서버에 스킨 적용 실패", "실패"); return; }

                _onlineSelectedSkinId = id;
                canvas.Invalidate();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "스킨 적용 실패");
            }
        }
        else
        {
            _selectedSkinId = id;
            SaveShopState();
            _solo?.SetCosmetic(CosmeticCatalog.InstanceOf(id)); // 싱글 즉시 반영
            canvas.Invalidate();
        }
    }

    async Task ApplyThemeSelectionAsync()
    {
        var id = (cmbTheme.SelectedItem as ComboItem)?.Id;
        if (string.IsNullOrEmpty(id)) return;

        if (_connected)
        {
            try
            {
                var ok = await _conn.InvokeAsync<bool>(
                    "SetSelection",
                    txtName.Text,
                    new SetSelectionRequest(CosmeticId: null, ThemeId: id)
                );
                if (!ok) { MessageBox.Show(this, "서버에 테마 적용 실패", "실패"); return; }

                _onlineSelectedThemeId = id;
                canvas.Invalidate();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "테마 적용 실패");
            }
        }
        else
        {
            _selectedThemeId = id;
            SaveShopState();
            canvas.Invalidate();
        }
    }

    record ComboItem(string Id, string Text) { public override string ToString() => Text; }
    class ShopState { public int Coins { get; set; } public List<string>? OwnedSkins { get; set; } public List<string>? OwnedThemes { get; set; } public string? SelectedSkinId { get; set; } public string? SelectedThemeId { get; set; } }


    // 서버와 동일한 필드명의 룸 채팅 DTO
    private record RoomChatDto(string At, string RoomId, string Sender, string Text);

    private record RosterItem(string PlayerId, bool IsHost);

    // ────────────────────────────────────────────────────────────────────────────
    // 싱글 엔진
    // ────────────────────────────────────────────────────────────────────────────
    sealed class SoloEngine
    {
        readonly int W, H;
        readonly Random r = new();
        readonly List<Point> body = new();
        CosmeticInstance cosmetic;
        Direction dir = Direction.Right;
        Point apple;
        int tick = 0;
        bool alive = true;
        readonly string name;

        class LootEntity { public int Id; public Point Pos; public int Amount; public LootKind Kind; public int DespawnAt; }
        readonly List<LootEntity> loot = new();
        int lootSeq = 1;
        int nextGoldTick = 0, nextXpTick = 0;
        const int GOLD_MIN = 12 * 15, GOLD_MAX = 12 * 30;
        const int XP_MIN = 12 * 12, XP_MAX = 12 * 24;
        const int LOOT_TTL = 12 * 20;

        int grow = 0;

        public record PickupEvent(LootKind Kind, int Amount, Point Cell);
        readonly List<PickupEvent> events = new();

        int boost = 0;

        public SoloEngine(int w, int h, string playerName, CosmeticInstance chosen)
        {
            W = w; H = h; name = playerName;
            cosmetic = chosen;
            body.Add(new Point(W / 2, H / 2));
            apple = RandApple();
            ScheduleNext();
            Rebuild();
        }

        public void SetBoost(int b) { boost = Math.Max(0, Math.Min(5, b)); }

        public void SetCosmetic(CosmeticInstance chosen)
        {
            cosmetic = chosen;
            Rebuild();
        }

        Point RandApple()
        {
            Point p;
            do { p = new Point(r.Next(0, W), r.Next(0, H)); } while (body.Contains(p) || loot.Any(L => L.Pos == p));
            return p;
        }

        public int Score => Math.Max(0, body.Count - 1);
        public int Level => 1 + Score / 5;
        public bool Alive => alive;

        public int RecommendedIntervalMs
        {
            get
            {
                int baseHz = Math.Min(30, 12 + Math.Max(0, (Level - 1) * 2));
                int hz = Math.Min(60, baseHz + 4 * boost);
                return Math.Max(12, 1000 / hz);
            }
        }

        public GameSnapshot Current { get; private set; } = default!;

        public void Update()
        {
            if (!alive) { Rebuild(); return; }
            tick++;

            SpawnLootIfNeeded();
            loot.RemoveAll(l => l.DespawnAt <= tick);

            var head = body[0];
            var next = new Point(head.X + dir.DX, head.Y + dir.DY);

            if (next.X < 0 || next.Y < 0 || next.X >= W || next.Y >= H)
            { alive = false; Rebuild(); return; }

            bool eatApple = (next == apple);
            bool consumeGrow = (!eatApple && grow > 0);
            bool willGrow = eatApple || consumeGrow;

            bool hitSelf = willGrow ? body.Contains(next) : body.Take(body.Count - 1).Contains(next);
            if (hitSelf) { alive = false; Rebuild(); return; }

            body.Insert(0, next);

            var hit = loot.FirstOrDefault(l => l.Pos == next);
            if (hit != null)
            {
                loot.Remove(hit);
                if (hit.Kind == LootKind.Gold)
                    events.Add(new PickupEvent(LootKind.Gold, hit.Amount, next));
                else
                {
                    grow += hit.Amount;
                    events.Add(new PickupEvent(LootKind.Xp, hit.Amount, next));
                }
            }

            if (eatApple) apple = RandApple();
            else if (consumeGrow) grow--;
            else body.RemoveAt(body.Count - 1);

            Rebuild();
        }

        void SpawnLootIfNeeded()
        {
            if (tick >= nextGoldTick) { Spawn(LootKind.Gold, r.Next(4, 9)); nextGoldTick = tick + r.Next(GOLD_MIN, GOLD_MAX + 1); }
            if (tick >= nextXpTick) { Spawn(LootKind.Xp, r.Next(2, 5)); nextXpTick = tick + r.Next(XP_MIN, XP_MAX + 1); }
        }
        void Spawn(LootKind kind, int amount)
        {
            var p = RandEmpty();
            loot.Add(new LootEntity { Id = lootSeq++, Pos = p, Amount = amount, Kind = kind, DespawnAt = tick + LOOT_TTL });
        }
        Point RandEmpty()
        {
            Point p;
            do { p = new Point(r.Next(0, W), r.Next(0, H)); } while (p == apple || body.Contains(p) || loot.Any(L => L.Pos == p));
            return p;
        }
        void ScheduleNext()
        {
            nextGoldTick = r.Next(GOLD_MIN, GOLD_MAX + 1);
            nextXpTick = r.Next(XP_MIN, XP_MAX + 1);
        }

        public void Apply(InputKey key)
        {
            var nd = key switch
            {
                InputKey.Up => Direction.Up,
                InputKey.Down => Direction.Down,
                InputKey.Left => Direction.Left,
                InputKey.Right => Direction.Right,
                _ => dir
            };
            if ((dir == Direction.Left && nd == Direction.Right) ||
                (dir == Direction.Right && nd == Direction.Left) ||
                (dir == Direction.Up && nd == Direction.Down) ||
                (dir == Direction.Down && nd == Direction.Up)) return;
            dir = nd;
        }

        void Rebuild()
        {
            var player = new PlayerStateDto(
                PlayerId: "solo:me",
                Name: name,
                Level: Level,
                Body: body.ToList(),
                Alive: alive,
                Score: Score,
                Cosmetic: cosmetic
            );
            var snap = new GameSnapshot("solo", tick, apple, new List<PlayerStateDto> { player }, MatchPhase.Playing, 999999)
            {
                Loot = loot.Select(l => new LootDropDto(l.Id, l.Pos, l.Amount, l.Kind)).ToList()
            };
            Current = snap;
        }

        public List<PickupEvent> DrainEvents()
        {
            var copy = events.ToList();
            events.Clear();
            return copy;
        }

        readonly record struct Direction(int DX, int DY, char Code)
        {
            public static Direction Up => new(0, -1, 'U');
            public static Direction Down => new(0, 1, 'D');
            public static Direction Left => new(-1, 0, 'L');
            public static Direction Right => new(1, 0, 'R');

            public static Direction FromCode(char c) => c switch
            {
                'U' => Up,
                'D' => Down,
                'L' => Left,
                'R' => Right,
                _ => Right
            };
        }

        public SoloSaveData ExportState() => new()
        {
            W = W,
            H = H,
            Name = name,
            Body = body.ToList(),
            Apple = apple,
            Dir = dir.Code,
            Tick = tick,
            Alive = alive
        };

        public void ImportState(SoloSaveData s)
        {
            body.Clear();
            body.AddRange(s.Body ?? new List<Point> { new(W / 2, H / 2) });
            apple = s.Apple == default ? RandApple() : s.Apple;
            dir = Direction.FromCode(s.Dir);
            tick = s.Tick;
            alive = s.Alive;
            loot.Clear(); ScheduleNext();
            grow = 0;
            Rebuild();
        }
    }

    internal sealed class SoloSaveData
    {
        public int W { get; set; }
        public int H { get; set; }
        public string Name { get; set; } = "Player";
        public List<Point>? Body { get; set; }
        public Point Apple { get; set; }
        public char Dir { get; set; } = 'R';
        public int Tick { get; set; }
        public bool Alive { get; set; } = true;
    }

    class GameCanvas : Panel
    {
        public GameCanvas()
        {
            DoubleBuffered = true;
            ResizeRedraw = true;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
            UpdateStyles();
        }
    }

    void AddFloater(Point grid, string text, LootKind kind) => _floaters.Add(new Floater(grid, text, kind));

    class Floater
    {
        public Point Grid;
        public string Text;
        public LootKind Kind;
        public int Age = 0;
        public int Life = 24;

        public Floater(Point g, string t, LootKind kind) { Grid = g; Text = t; Kind = kind; }
        public bool Tick() { Age += 1; return Age <= Life; }
    }
}
