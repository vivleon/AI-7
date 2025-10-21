from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.text import Text
import os

console = Console()

def clear():
    os.system("cls" if os.name == "nt" else "clear")



os.system("clear")  # 페이지 변환

import time
import datetime
import random

today = datetime.datetime.today() + datetime.timedelta(hours=9) # 현재 날짜
week_day = today.weekday() # 현재 주의 토요일을 구하기 위해 현재 요일(0: 일, 1: 월, ..., 6: 토)을 확인
lst_week = ["(일)", "(월)", "(화)", "(수)", "(목)", "(금)", "(토)"]
today_week = lst_week[week_day]
today_2 = today.strftime("%Y-%m-%d")
today_3 = today.strftime("%H:%M:%S")

print_date = today_2, today_week, today_3


def starlight_animation(rows=10, cols=60, duration=1.5):  #별을 랜덤으로 출력하면서 반짝거리는 효과를 넣으려 했으나 너무 부잡해서 활용하지는 않음
    frames = int(duration / 0.1)
    for _ in range(frames):
        clear()
        for _ in range(rows):
            line = ''
            for _ in range(cols):
                char = random.choice([' ', ' ', ' ', '*', '.', '✦', '★', '☆'])
                line += char
            print('\033[95m' + line + '\033[0m')  # 한 줄 완성 후 출력
        time.sleep(0.1)


def start_screen():
    starlight_animation()

def print_banner(): #모든 페이지에서 활용하기위해 배너를 수정하고 테두리를 입혔음. 가시효과를 위해 색상 또한 변경
    banner_text = Text()
    banner_text.append("\n\n                🍦🍧🍨 Welcome to 🍨🍧🍦        \n\n", style="bold magenta")
    banner_text.append("      ██████╗  █████╗  ███████╗██╗  ██╗██╗███╗   ██╗\n", style="bold yellow")
    banner_text.append("      ██   ██╗ ██╔══██╗██╔════╝██║ ██║ ██║████╗  ██║\n", style="bold yellow")
    banner_text.append("      ████████║███████║███████╗█████║  ██║██╔██╗ ██║\n", style="bold yellow")
    banner_text.append("      ██║   ██║██╔══██║╚════██║██╔══██║██║██║╚██╗██║\n", style="bold yellow")
    banner_text.append("      ███████╔╝██║  ██║███████║██║  ██║██║██║ ╚████║\n", style="bold yellow")
    banner_text.append("      ╚══════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝\n\n", style="bold yellow")
    banner_text.append("                🍨 Baskin Robbins 31 🍨         \n", style="bold cyan")
    banner_text.append("                   ★ Order Kiosk ★\n\n", style="bold bright_yellow")

    console.print(Panel(banner_text, border_style="magenta", width=60))
import os

import unicodedata
# 문자열의 너비를 맞추는 함수
def preFormat(string, width, align='<', fill=' '):
    count = (width - sum(1 + (unicodedata.east_asian_width(c) in "WF") for c in string))
    return {
        '>': lambda s: fill * count + s,  # 오른쪽 정렬
        '<': lambda s: s + fill * count,  # 왼쪽 정렬
        '^': lambda s: fill * (count // 2)  # 가운데 정렬
                       + s
                       + fill * (count // 2 + count % 2)
    }[align](string)

def show_main_page():
    clear()
    print("\n" + "=" * 60)
    print_banner()
    print("=" * 60)
    print("\n\n\n\n\n        ✨ Baskin Robbins에 오신 걸 환영합니다! ✨\n")
    print("\n\n    맛있는 아이스크림이 여러분을 기다리고 있어요! 🍦🍓🍫\n\n\n\n")
    print("=" * 60)
    print("=" * 60)
    time.sleep(0.1)
    input(f'{"        >>> 🍦 엔터를 눌러 주문을 시작하세요<<<":<60}')

def top_banner():
    print("=" * 60)
    print("\n        ✨ Baskin Robbins에 오신 걸 환영합니다! ✨\n")
    print("\n    맛있는 아이스크림이 여러분을 기다리고 있어요! 🍦🍓🍫\n")
    print("=" * 60)
    time.sleep(0.1)


import Menu


def show_categories():
    table = Table(title="\n📋 메뉴 카테고리 📋 \n", title_style="bold cyan", box=None, show_edge=False, show_lines=False)
    table.add_column("번호", style="bold yellow", justify="center")
    table.add_column("   카테고리", style="bold green")

    for idx, category in enumerate(Menu.categories, 1):
        table.add_row(str(idx), category)
    console.print(table)


def choose_categories():
    show_categories()
    while True:
        try:
            num = int(input("원하는 카테고리 번호를 선택하세요. >> "))
            if 1 <= num <= len(Menu.categories):
                return Menu.categories[num - 1]
            else:
                print("범위 안의 숫자를 입력하세요. >> ")
        except ValueError:
            print("숫자를 입력하세요.")


cup_cone = ''

def print_menu(category_menu, category_name): #표로 출력됨으로써 출력화면이 깔끔하게 만듦
    table = Table(title=f"{category_name} 메뉴 선택", title_style="bold magenta")
    table.add_column("번호", style="bold yellow", justify="center")
    table.add_column("메뉴명", style="green")
    table.add_column("가격", style="bold cyan", justify="right")

    for i, (name, price) in enumerate(category_menu.items(), 1): # 이전보다 좀 더 깔끔하게 출력될 수 있도록 함
        table.add_row(str(i), name, f"{price:,} 원")

    console.print(table)

def choose_menu(category_menu, category_name):
    global cup_cone
    global count
    while True:
        os.system("clear")
        print_banner()
        print_menu(category_menu, category_name)
        items = list(category_menu.items())

        try:
            num = int(input("원하는 메뉴를 선택하세요 >> "))
            if 1 <= num <= len(category_menu):
                item = items[num - 1]
                name = item[0]
                price = item[1]
                count = int(input("구매할 개수를 선택해주세요 >> "))

                if category_name == "🍦 아이스크림":
                    if name == '싱글 레귤러 - 한가지 맛 (중량 115g)' or name == '싱글 킹 - 한가지 맛 (중량 145g)' or name == '더블 주니어 - 두가지 맛 (중량 150g)' or name == '더블 레귤러 - 두가지 맛 (중량 230g)':
                        sel = int(input("컵/콘 선택(1. 컵/ 2. 콘) >>"))
                        if sel == 1:
                            cup_cone = '(컵)'
                        elif sel == 2:
                            cup_cone = '(콘)'
                    else:
                        cup_cone = '(핸드팩)' # 아이스크림 중에서도 핸드팩 옵션일때만 스푼과 드라이아이스를 입력받음
                        if order_option == '[ 가 져 가 기 ]':
                            spoon = input("스푼 개수를 입력해주세요 >> ")
                            dry_ice = input("드라이 아이스를 선택해주세요(y/n) >> ")
                        elif order_option == '[ 매 장 이 용 ]': #매장이용을 선택했을 경우에는 드라이아이스를 입력받지 않도록 함.
                            spoon = input("스푼 개수를 입력해주세요 >> ")

                        else:
                            print("드라이아이스는 포장시에만 적용됩니다.")

                    flavors = choose_flavor(Menu.icecream_count[name], count)  # 선택된 맛 저장
                    name = name + cup_cone  # 컵 또는 콘에 대한 선택을 이름에 추가
                    order_list.append((name, price, count, flavors))  # 맛도 함께 저장
                    print(flavors)
                else:
                    order_list.append((name, price, count, []))  # 아이스크림 외의 항목은 맛 없이 추가

                os.system("clear")
                print_banner()
                again = input("같은 카테고리에서 추가로 주문하시겠습니까? (y/n) >> ").strip().lower()
                if again != 'y':
                    break
            else:
                print("범위 안의 숫자를 입력하세요.")
        except ValueError:
            print("숫자를 입력하세요.")


def show_menu(categories):
    print(f" ★ {categories} 메뉴 선택 ★ \n")
    items = list(categories.items())
    for i, (name, price) in enumerate(items, 1):
        print(f"{i}. {name} : {price:,} 원")


def choose_flavor(size, count):
    os.system("clear")
    print_banner()
    print(f"\n{'== 옵션 ==':^60} ")
    flavors = []
    total_count = size * count
    for i, flavor in enumerate(Menu.icecream_flavors, 1):
        print(f"{i}. {flavor}")

    i = 1
    while True:
        try:
            num = int(input(f"{i}번째 맛을 선택하세요.({total_count}개까지 선택가능) >> "))
            if 1 <= num <= len(Menu.icecream_flavors):
                flavors.append(Menu.icecream_flavors[num - 1])
                i = i + 1
                if (i > total_count):
                    return flavors
            else:
                print("범위 안의 숫자를 입력하세요")
        except ValueError:
            print("숫자를 입력하세요.")
        print(flavors)


order_list = []


#주문확인
def print_ordercheck_lst(orderList):
    clear()
    print_banner()
    total_price = 0

    table = Table(title="== 주문확인 ==", title_style="bold cyan") # 주문확인 페이지를 표로 출력되게 함으로써 깔끔함을 강조
    table.add_column("번호", style="bold yellow", justify="center")
    table.add_column("메뉴명(옵션)", style="bold green")
    table.add_column("맛", style="magenta")
    table.add_column("단가", style="cyan", justify="right")
    table.add_column("수량", style="yellow", justify="center")
    table.add_column("합계", style="bold red", justify="right")

    for idx, (name, price, count, flavors) in enumerate(orderList, 1):
        flavor_str = ", ".join(flavors) if flavors else "옵션없음"
        total = price * count
        total_price += total
        table.add_row(str(idx), name, flavor_str, f"{price:,} 원", str(count), f"{total:,} 원")

    console.print(table)
    console.print(f"\n▶ 총 결제 금액 : [bold red]{total_price:,} 원[/bold red]\n") #총 결제금액을 붉은 글씨로 표현하여 가독성을 높임


def apply_discount_or_point():
    global point_name
    global discount_name
    global dis_pnt_num
    os.system("clear")
    print_banner()
    total_sum = 0
    from Menu import happy_num, kt_num, t_num, blu_mem_num, worker_id
    global opt
    for item in order_list:
        name, price, count, flavors = item  # item에서 4개의 값을 언패킹
        total_sum += price * count

    while True:
        try:
            sel = input("\n 할인/ 적립을 하시겠습니까? (y/n) >> ")
            if sel == 'y':
                opt = int(input(
                    "옵션 선택\n 1. 해피포인트 \n 2. KT 멤버십 \n 3. T 멤버십 \n 4. 블루멤버쉽\n 5. 임직원\n 6. 할인/적립 안함 \n 옵션을 선택해 주세요 >> "))
                if opt == 1:
                    num = int(input("해피포인트 번호를 입력해 주세요 >> "))
                    if num in happy_num: #리스트의 번호와의 검증을 거치도록 함. 다른 회원번호들도 동일
                        print("해피포인트가 적립되었습니다.")
                        point_name = '해피포인트'
                        discount_name = ''
                        discount = 0
                        dis_pnt_num = num
                        point = int(total_sum * 0.05)
                        return discount, point, point_name, discount_name
                elif opt == 2:
                    num = int(input("KT 멤버십 번호를 입력해 주세요 >> "))
                    if num in kt_num:
                        print("KT 멤버십 할인이 적용되었습니다.")
                        point_name = ''
                        discount_name = 'KT 멤버십'
                        discount = int(total_sum * 0.05)
                        dis_pnt_num = num
                        point = 0
                        return discount, point, point_name, discount_name
                elif opt == 3:
                    num = int(input("T 멤버십 번호를 입력해 주세요 >> "))
                    if num in t_num:
                        print("T 멤버십 할인이 적용되었습니다.")
                        point_name = ''
                        discount_name = 'T 멤버십'
                        discount = int(total_sum * 0.05)
                        dis_pnt_num = num
                        point = 0
                        return discount, point, point_name, discount_name
                elif opt == 4:
                    num = int(input("블루멤버스십 번호를 입력해 주세요 >> "))
                    if num in blu_mem_num:
                        point_name = ''
                        discount_name = '블루멤버스'
                        discount = int(total_sum * 0.05)
                        dis_pnt_num = num
                        point = 0
                        return discount, point, point_name, discount_name
                elif opt == 5:
                    num = int(input("임직원 번호를 입력해 주세요 >> "))
                    if num in worker_id:
                        print("임직원 할인이 적용되었습니다.")
                        point_name = ''
                        discount_name = '임직원 할인'
                        discount = int(total_sum * 0.5)
                        dis_pnt_num = num
                        point = 0
                        return discount, point, point_name, discount_name
                elif opt == 6:
                    dis_pnt_num = ""
                    return 0, 0, '', ''
            else:
                print("할인/적립을 진행하지 않습니다.")
                dis_pnt_num = ""
                return 0, 0, '', ''
        except ValueError:
            print("잘못 입력하셨습니다.")


def luhn_check(card_number): # 카드번호가 실제 카드발급 로직과 맞는지 검증하도록 함.
    total = 0
    reverse_digits = card_number[::-1]

    for i, digit in enumerate(reverse_digits):
        n = int(digit)
        if i % 2 == 1:
            n = n * 2
            if n > 9:
                n -= 9
        total += n

    return total % 10 == 0


def get_card_type(card_number): #카드 앞자리에 따라 어느 회사가 발급한 카드인지를 도출할 수 있도록  함
    if card_number.startswith('4'):
        return "VISA"
    elif card_number.startswith(('51', '52', '53', '54', '55')) or \
            (2221 <= int(card_number[:4]) <= 2720):
        return "MasterCard"
    elif card_number.startswith(('34', '37')):
        return "American Express"
    elif card_number.startswith(('35', '36', '38', '39')):
        return "BC (or JCB/Diners 등, 수동 판별 필요)"
    else:
        return "Unknown"


def next_screen():
    print("\n[영수증 출력중입니다... ✅]")
    time.sleep(1.0) # 딜레이를 줌으로써 출력시간을 만들어줌.




def validate_card_and_proceed(): #카드가 유효한지 아닌지를 표시하도록 함
    global payment_num
    global card_type
    card_number = input("IC카드를 삽입하세요 \n: ").replace(" ", "")
    payment_num = card_number
    if not card_number.isdigit():
        print("❌ 숫자만 입력해 주세요.")
        return False

    if len(card_number) not in [15, 16]: #AMEX는 15자리 이므로 15자리 16자리를 모두 수용하도록 설정
        print("❌ 카드 번호는 15자리 또는 16자리여야 합니다.")
        return False

    card_type = get_card_type(card_number)

    if not luhn_check(card_number):
        print(f"❌ 유효하지 않은 카드 번호입니다. ({card_type})")
        return False

    print(f"✅ 카드 정보가 유효합니다.\n카드사: {card_type}")
    time.sleep(1.0)
    next_screen()
    return True



def payment():
    os.system("clear")
    print_banner()
    global payment_type
    global payment_num
    global card_type
    from Menu import payment_opt_add_coup
    while True:
        console.print("[bold green]결제 방법을 선택해 주세요.[/bold green]")
        print(" 1. 신용카드  \n 2. 현금  \n 3. 모바일 교환권  \n 4. 삼성페이/애플페이 \n 5. 카카오페이 \n 6. 네이버페이 \n 7. 페이코  \n")
        try:
            payment_num = int(input(" >> "))
            if payment_num == 1:
                success = validate_card_and_proceed()
                if success:
                    payment_type = "[ 신 용 카 드 ]"
                    break  # 카드 유효 → 루프 탈출

            elif payment_num == 3:
                try:
                    payment_opt_add = int(input("코드를 입력하세요 \n >> "))
                except ValueError:
                    print("숫자만 입력해 주세요.")
                    continue
                if payment_opt_add in payment_opt_add_coup:
                    payment_type = "[ 모바일 교환권 ]"
                    payment_num = payment_opt_add
                    card_type = "M-CPN"
                    print("할인이 적용되었습니다.")
                    time.sleep(1.0)
                    break
                else:
                    print("잘못된 코드입니다.")
            else:
                print("잘못 입력되었습니다. 다시 시도해 주세요.")
        except ValueError:
            print("숫자만 입력해 주세요.")
            continue


def receipt(discount, point, point_name, discount_name):
    from Menu import dis_opt, payment_type_lst
    if dis_pnt_num:
        masked_number = str(dis_pnt_num)[:3] + "****" + str(dis_pnt_num)[5:]  #포인트카드 번호 그리고 신용카드 번호 등의 일부를 *로 표기
    masked_number_2 = str(payment_num)[:4] + "*******" + str(payment_num)[8:]
    os.system("clear")
    print(f"\n\n\n\033[1;96m{'🍨 Baskin Robbins 31 🍨':^60}\033[0m\n\n")

    print("-" * 60)
    print(f"\n\033[1m{order_option:^60}\033[0m\n")
    print("-" * 60)
    print(" 광주소촌")
    print(" 대표자: 홍길동")
    print(" Tel 062-942-7777")
    print(" 광주광역시 광산구 소촌로 144, 남경빌딩 1층")
    print("-" * 60)
    print("소비자 중심 경영 인증기업(CCM)")
    print("-" * 60)
    print("[ 구 매 일 시 ]", end=" ")
    for time_today in range(0,3):
        print(print_date[time_today], end = " ")
    print()
    print("-" * 60)
    print(f"{' 상품명(옵션)':^28}{'가격':<7} {'개수':<5} {' 합계'}")  # '맛' 컬럼 추가
    print("-" * 60)
    total_sum = 0
    for item in order_list:
        name, price, count, flavors = item
        total = price * count
        total_sum += total
        flavors_str = "\n└".join(flavors) if flavors else " 옵션없음"  # 선택한 맛이 있다면, 맛을 출력
        print(f"\n* {preFormat(name[0:15], 28, '<')} {preFormat(f'{price:,} 원', 10, '<')} {preFormat(str(count), 6, '^')} {preFormat(f'{total: ,} 원', 10, '>')}")
        print(f"└{flavors_str}")

    print("-" * 60)
    print(preFormat(' 총합계금액', 40, '<'), preFormat(f"{total_sum: ,}", 15, '>'), "원") # preFormat을 사용하여 한글 간격을 맞춤
    print(preFormat(' 총할인금액', 40, '<'), preFormat(f"{discount: ,}", 15, '>'), "원")
    if discount_name:
        print(f" ({discount_name})")
    print(preFormat(' 포인트적립', 40, '<'), preFormat(f"{point:,.0f}", 15, '>'), "점")
    if point_name:
        print(f" ({point_name})")
    print(preFormat(' 총결제금액', 40, '<'), preFormat(f"{total_sum - discount: ,}", 15, '>'), "원")
    if point_name == '해피포인트':
        print("-" * 60)
        print(f" {'■ 할인 / 적립':<18} {dis_opt[0]:>28}")
        print(f" {masked_number:<46} {point:>8,.0f} 점") # 1234****90 출력
    if discount_name == 'KT 멤버십':
        print("-" * 60)
        print(f" {'■ 할인 / 적립':<19}  {dis_opt[1]:>28}")
        print(f" {masked_number:<46}  {discount:>8,} 원")
    if discount_name == 'T 멤버십':
        print("-" * 60)
        print(f" {'■ 할인 / 적립':<19}  {dis_opt[2]:>28}")
        print(f" {masked_number:<46}  {discount:>8,} 원")
    if discount_name == '블루멤버스':
        print("-" * 60)
        print(f" {'■ 할인 / 적립':<19}  {dis_opt[3]:>28}")
        print(f" {masked_number:<46}  {discount:>8,} 원")
    if discount_name == '임직원 할인':
        print("-" * 60)
        print(f" {'■ 할인 / 적립':<19}  {dis_opt[4]:>28}")
        print(f" {masked_number:<46}  {discount:>8,} 원")
    if payment_type:
        if payment_type in payment_type_lst:
            print("-" * 60)
            print(f" ■ {payment_type:<20}  {card_type:>28}")
            print(f" {masked_number_2:<46}  {total_sum:>8,} 원")

    print("-" * 60)
    print(f"\n\033[1m{"♥ Thank you ♥":^60}\033[0m\n")
    print("-" * 60)

    # 주문목록 초기화
    order_list.clear()


def take_order():
    category_to_menu = {
        "🍦 아이스크림": Menu.icecream_menu,
        "☕ 커피": Menu.coffee_menu,
        "🍰 케이크": Menu.icecream_cake_menu,
        "🍮 디저트": Menu.dessert_menu
    }

    while True:
        category_name = choose_categories()

        menu = category_to_menu.get(category_name)
        if menu:
            choose_menu(menu, category_name)
        else:
            print("잘 못 입력 되었습니다.")

        again = input("다른 상품을 추가로 주문하시겠습니까? (y/n) >> ").strip().lower()
        if again != 'y':
            print("주문이 완료되었습니다.")
            break

def main():
    global order_option
    while True:
        show_main_page()
        clear()

        print_banner()
        num = int(input("\n 포장하실지 매장에서 드실지를 선택하여 주세요. \n \n 1. 🚗 가져가기 \n \n 2. 🏪 매장이용 \n \n>> "))
        while True:
            try:
                if num == 1:
                    order_option = '[ 가 져 가 기 ]'
                    break
                elif num == 2:
                    order_option = '[ 매 장 이 용 ]'
                    break
                else:
                    print("범위 안의 숫자를 입력해주세요.")
            except ValueError:
                print("숫자를 입력하세요.")


        take_order() # 오더페이지

        #주문 확인 화면단 출력,
        print_ordercheck_lst(order_list)

        #주문 취소하면 취소 진행, 결제하기 입력하면 break로 빠져나와서 다음 코드 진행,
        while True:
            print("-" * 60)
            ordercheck_result = int(input("주문을 완료하시겠습니까? 1. 결제하기  2. 주문수정 \n>> "))

            # 주문취소를 입력
            if ordercheck_result == 2:
                delNum = int(input("삭제할 상품의 번호를 입력해주세요. (0: 주문수정 취소) \n>> "))

                # 0번 입력시 결제하기/주문취소 다시선택
                # 상품 번호 입력시에 해당 상품 리스트에서 삭제
                if delNum != 0:
                    del order_list[delNum - 1]
                    print_ordercheck_lst(order_list)
            else:
                break

        discount, point, point_name, discount_name = apply_discount_or_point()  # 포인트와 총합계 계산

        payment()

        receipt(discount, point, point_name, discount_name)  # 계산된 값을 receipt 함수로 전달
        # 반복 여부 확인

        # 초기화면 호출
        input("엔터를 누르면 초기화면으로 돌아갑니다...")


if __name__ == "__main__":
    main()