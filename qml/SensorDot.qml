import QtQuick 2.15

Item {
    id: root
    property int level: 0   // 0: 정상, 1: 주의, 2: 경고, 3: 위험
    width: 24
    height: 24

    Rectangle {
        id: circle
        anchors.centerIn: parent
        width: 20
        height: 20
        radius: width / 2
        border.color: "black"
        border.width: 1.5

        color: {
            switch(root.level) {
                case 0: return "green";   // 정상
                case 1: return "yellow";  // 주의
                case 2: return "orange";  // 경고
                case 3: return "red";     // 위험
                default: return "gray";
            }
        }
    }
}
