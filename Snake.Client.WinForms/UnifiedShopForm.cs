// Snake.Client.WinForms/UnifiedShopForm.cs
using System;
using System.Drawing;
using System.Windows.Forms;
using Microsoft.AspNetCore.SignalR.Client;
using Snake.Shared;

namespace Snake.Client.WinForms
{
    public class UnifiedShopForm : Form
    {
        private readonly HubConnection? _conn;
        private readonly string _playerName;

        // 로컬 코인/구매 델리게이트
        private readonly Func<int> _getLocalCoins;
        private readonly Action<int> _setLocalCoins;
        private readonly Func<string, int, bool, bool> _localTryBuy;

        public UnifiedShopForm(
            HubConnection? conn,
            string playerName,
            Func<int> getLocalCoins,
            Action<int> setLocalCoins,
            Func<string, int, bool, bool> localTryBuy)
        {
            _conn = conn;
            _playerName = playerName;
            _getLocalCoins = getLocalCoins;
            _setLocalCoins = setLocalCoins;
            _localTryBuy = localTryBuy;

            Text = "통합 상점";
            StartPosition = FormStartPosition.CenterParent;
            Width = 980; Height = 720;

            var tabs = new TabControl { Dock = DockStyle.Fill };
            var tpOnline = new TabPage("온라인 상점");
            var tpLocal = new TabPage("싱글 상점");
            var tpHelp = new TabPage("Help");

            // ── 온라인 상점 임베드 (연결 안 되어 있으면 안내)
            if (_conn != null && _conn.State == HubConnectionState.Connected)
            {
                var fOnline = new OnlineShopForm(_conn, _playerName)
                {
                    TopLevel = false,
                    FormBorderStyle = FormBorderStyle.None,
                    Dock = DockStyle.Fill
                };
                tpOnline.Controls.Add(fOnline);
                fOnline.Show();
            }
            else
            {
                var info = new Label
                {
                    Dock = DockStyle.Fill,
                    Text = "온라인 상점은 로그인 후 이용할 수 있습니다.",
                    TextAlign = ContentAlignment.MiddleCenter
                };
                tpOnline.Controls.Add(info);
            }

            // ── 로컬 상점 임베드 (싱글 전용)
            {
                var fLocal = new LocalShopForm(_getLocalCoins, _setLocalCoins, _localTryBuy)
                {
                    TopLevel = false,
                    FormBorderStyle = FormBorderStyle.None,
                    Dock = DockStyle.Fill
                };
                tpLocal.Controls.Add(fLocal);
                fLocal.Show();
            }

            // ── Help 탭
            tpHelp.Padding = new Padding(8);
            var help = new RichTextBox
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                BorderStyle = BorderStyle.None,
                DetectUrls = false,
                ScrollBars = RichTextBoxScrollBars.Vertical
            };
            help.Text =
@"[통합 상점 가이드]

■ 탭 구성
- 온라인 상점: 서버 계정 코인/샤드로 가챠, 제작, 스킨/테마 구매.
  · 요구 레벨 미충족/코인 부족/보유중은 버튼 비활성 및 툴팁 사유 표시.
  · 카탈로그는 잠금이어도 항상 노출됨(조건은 카드에 표기).

- 싱글 상점: 싱글플레이 전용 코인으로 로컬 구매(오프라인 인벤토리).
  · 로컬 코인은 싱글 게임에서 사과/드랍으로 획득. 
  · 요구 레벨은 참고용 표기로만 노출(오프라인은 레벨 제한 없음).

■ 가챠 (온라인)
- 1회 50c, 10연 450c.
- 희귀도 확률: Common 62%, Rare 25%, Epic 10%, Legendary 3%.
- 중복 시 희귀도별 샤드 환급 (예: Epic 75, Legendary 250).
- 피티: 10연에 Rare 보장, 30연에 Epic 보장(풀에 없으면 인접 희귀도 보정).

■ 제작 (온라인)
- 희귀도별 샤드 비용: Common 120 / Rare 300 / Epic 600 / Legendary 900.
- 이미 보유한 경우 제작 불가.

■ 스킨/테마 (온라인/싱글 공통 정책)
- 아이템은 전부 카탈로그에 노출.
- 카드에 '요구 레벨', '가격', '보유 여부' 표기.
- 구매 불가 사유(레벨/코인)는 버튼 툴팁으로 표시.

■ 코인/샤드
- 온라인 코인/샤드는 서버 계정 자산이며 매치 보상/가챠/제작과 연동.
- 싱글 코인은 로컬 파일로 저장(solo_shop.json).";

            tpHelp.Controls.Add(help);

            tabs.TabPages.Add(tpOnline);
            tabs.TabPages.Add(tpLocal);
            tabs.TabPages.Add(tpHelp);
            Controls.Add(tabs);
        }
    }
}
