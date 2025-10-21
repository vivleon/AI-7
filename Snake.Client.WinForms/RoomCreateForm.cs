// Snake.Client.WinForms/RoomCreateForm.cs
using System;
using System.Windows.Forms;

namespace Snake.Client.WinForms;

public class RoomCreateForm : Form
{
    readonly TextBox tbTitle = new() { Dock = DockStyle.Top, PlaceholderText = "방 제목(고유)" };
    readonly CheckBox cbPrivate = new() { Dock = DockStyle.Top, Text = "비공개 방" };
    readonly TextBox tbPwd = new() { Dock = DockStyle.Top, PlaceholderText = "비밀번호(비공개일 때)", UseSystemPasswordChar = true, Visible = false };

    readonly Button btnOk = new() { Dock = DockStyle.Bottom, Text = "생성" };
    readonly Button btnCancel = new() { Dock = DockStyle.Bottom, Text = "취소" };

    public string TitleText => tbTitle.Text.Trim();
    public bool IsPrivate => cbPrivate.Checked;
    public string? Password => IsPrivate ? (tbPwd.Text ?? "") : null;

    public RoomCreateForm()
    {
        Text = "방 만들기";
        Width = 360; Height = 180;
        StartPosition = FormStartPosition.CenterParent;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false; MinimizeBox = false;

        Controls.Add(btnOk);
        Controls.Add(btnCancel);
        Controls.Add(tbPwd);
        Controls.Add(cbPrivate);
        Controls.Add(tbTitle);

        cbPrivate.CheckedChanged += (_, __) => tbPwd.Visible = cbPrivate.Checked;

        btnOk.Click += (_, __) =>
        {
            if (string.IsNullOrWhiteSpace(tbTitle.Text))
            {
                MessageBox.Show(this, "방 제목을 입력하세요.", "확인");
                tbTitle.Focus();
                return;
            }
            if (tbTitle.Text.Contains(":"))
            {
                MessageBox.Show(this, "방 제목에는 콜론(:)을 사용할 수 없습니다.", "확인");
                tbTitle.Focus();
                return;
            }
            if (IsPrivate && string.IsNullOrEmpty(tbPwd.Text))
            {
                MessageBox.Show(this, "비공개 방의 비밀번호를 입력하세요.", "확인");
                tbPwd.Focus();
                return;
            }
            DialogResult = DialogResult.OK;
        };
        btnCancel.Click += (_, __) => DialogResult = DialogResult.Cancel;
    }
}
