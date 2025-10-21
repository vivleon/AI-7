// Snake.Client.WinForms/LoginForm.cs
using Microsoft.AspNetCore.SignalR.Client;
using Snake.Shared;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;

namespace Snake.Client.WinForms;

public class LoginForm : Form
{
    public string LoginId => txtId.Text.Trim();
    public string Password => txtPw.Text;
    public string FinalUrl { get; private set; } = "";

    // URL은 편집 불가 콤보 + 추가 버튼
    readonly ComboBox cbUrl = new() { Dock = DockStyle.Top, DropDownStyle = ComboBoxStyle.DropDownList };
    readonly Button btnAddUrl = new() { Dock = DockStyle.Top, Text = "URL 추가..." };

    readonly TextBox txtId = new() { Dock = DockStyle.Top, PlaceholderText = "아이디" };
    readonly TextBox txtPw = new() { Dock = DockStyle.Top, PlaceholderText = "비밀번호", UseSystemPasswordChar = true };
    readonly Button btnLogin = new() { Dock = DockStyle.Top, Text = "로그인" };
    readonly Button btnRegister = new() { Dock = DockStyle.Top, Text = "회원가입..." };
    readonly Label lblInfo = new()
    {
        Dock = DockStyle.Top,
        Text = "URL은 드롭다운에서 선택합니다.",
        AutoSize = true
    };

    // 기본 URL 목록
    readonly UrlItem[] defaultUrls =
    {
        new("사내 IP",    "http://10.10.21.114:5000/hub"),
        new("외부 공인IP", "http://61.108.2.68:5000/hub"),
        new("로컬 HTTP",  "http://localhost:5000/hub"),
        new("로컬 HTTPS", "https://localhost:5001/hub"),
    };

    public LoginForm()
    {
        Text = "로그인";
        Width = 380; Height = 260;
        StartPosition = FormStartPosition.CenterParent;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = MinimizeBox = false;

        Controls.Add(btnRegister);
        Controls.Add(btnLogin);
        Controls.Add(txtPw);
        Controls.Add(txtId);
        Controls.Add(btnAddUrl);
        Controls.Add(cbUrl);
        Controls.Add(lblInfo);

        cbUrl.Items.AddRange(defaultUrls);
        cbUrl.SelectedIndex = 0;

        // Tab 순서(위→아래)
        cbUrl.TabIndex = 0;
        btnAddUrl.TabIndex = 1;
        txtId.TabIndex = 2;
        txtPw.TabIndex = 3;
        btnLogin.TabIndex = 4;
        btnRegister.TabIndex = 5;

        AcceptButton = btnLogin;

        btnAddUrl.Click += (_, __) => OnAddUrl();

        btnLogin.Click += async (_, __) =>
        {
            btnLogin.Enabled = false;
            try
            {
                var selectedUrl = (cbUrl.SelectedItem as UrlItem)?.Url ?? defaultUrls[0].Url;

                // 선택 URL + (localhost면) 사내 IP 자동 재시도
                var tryUrls = new List<string> { selectedUrl };
                if (selectedUrl.Contains("localhost", StringComparison.OrdinalIgnoreCase))
                    tryUrls.Add("http://10.10.21.114:5000/hub");

                Exception? last = null;

                foreach (var u in tryUrls.Distinct(StringComparer.OrdinalIgnoreCase))
                {
                    try
                    {
                        await using var tmp = new HubConnectionBuilder()
                            .WithUrl(u)
                            .WithAutomaticReconnect()
                            .Build();

                        await tmp.StartAsync();

                        // 서버: Task<LoginResultDto> Login(string loginId, string password)
                        var login = await tmp.InvokeAsync<LoginResultDto>("Login", LoginId, Password);

                        FinalUrl = u;
                        DialogResult = DialogResult.OK;
                        return;
                    }
                    catch (Exception ex) { last = ex; }
                }

                MessageBox.Show(this, last?.Message ?? "로그인 실패", "실패");
            }
            finally { btnLogin.Enabled = true; }
        };

        btnRegister.Click += async (_, __) =>
        {
            using var rf = new RegisterForm();
            if (rf.ShowDialog(this) != DialogResult.OK) return;

            try
            {
                var u = (cbUrl.SelectedItem as UrlItem)?.Url ?? defaultUrls[0].Url;
                await using var tmp = new HubConnectionBuilder().WithUrl(u).Build();
                await tmp.StartAsync();

                var res = await tmp.InvokeAsync<PurchaseResultDto>("Register",
                    new RegisterDto(rf.LoginId, rf.Password, rf.PasswordConfirm, rf.RealName, rf.Gender, rf.Nick));

                MessageBox.Show(this, res.Message, res.Ok ? "OK" : "실패");
                if (res.Ok) { txtId.Text = rf.LoginId; txtPw.Text = rf.Password; }
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "회원가입 실패");
            }
        };
    }

    void OnAddUrl()
    {
        using var f = new Form
        {
            Text = "URL 추가",
            StartPosition = FormStartPosition.CenterParent,
            Width = 460,
            Height = 150,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false,
            MinimizeBox = false
        };
        var tb = new TextBox { Dock = DockStyle.Top, PlaceholderText = "http://host:port/hub" };
        var row = new FlowLayoutPanel { Dock = DockStyle.Bottom, FlowDirection = FlowDirection.RightToLeft, Height = 42 };
        var ok = new Button { Text = "추가", DialogResult = DialogResult.OK, Width = 88 };
        var cancel = new Button { Text = "취소", DialogResult = DialogResult.Cancel, Width = 88 };
        row.Controls.Add(ok); row.Controls.Add(cancel);
        f.Controls.Add(row); f.Controls.Add(tb);

        if (f.ShowDialog(this) == DialogResult.OK)
        {
            var url = (tb.Text ?? "").Trim();
            if (url.Length > 0)
            {
                var item = new UrlItem(url, url);
                cbUrl.Items.Add(item);
                cbUrl.SelectedItem = item;
            }
        }
    }

    private sealed record UrlItem(string Name, string Url)
    {
        public override string ToString() => Name;
    }
}
