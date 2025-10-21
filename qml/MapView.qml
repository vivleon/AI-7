import QtQuick 2.15
import QtQuick.Controls 2.15
import QtLocation 6.5
import QtPositioning 6.5

Item {
    id: root
    width: 720
    height: 520

    // 외부에서 호출
    property double centerLat: 37.579617
    property double centerLng: 126.977041
    property int    zoomLevel: 16

    function centerOnCoordinate(lat, lng) {
        centerLat = lat; centerLng = lng
        map.center = QtPositioning.coordinate(lat, lng)
        map.zoomLevel = 18
    }
    function setInfo(text) { infoLabel.text = text }

    // 마커/클러스터 간단 지원
    property var markers: []
    property var clustered: []

    // ✅ OSM 고정: API Key 경고 회피 (providersrepository 비활성)
    Plugin {
        id: mapPlugin
        name: "osm"
        PluginParameter { name: "osm.mapping.providersrepository.disabled"; value: true }
        PluginParameter { name: "osm.mapping.highdpi_tiles"; value: true }
        PluginParameter { name: "osm.mapping.host"; value: "https://a.basemaps.cartocdn.com/light_all/" }
        PluginParameter { name: "osm.useragent"; value: "VigilEdge/0.1 (contact@vigiledge.local)" }
        PluginParameter { name: "osm.mapping.copyrightsVisible"; value: true }
    }

    // 간단 클러스터링
    function haversineMeters(lat1, lon1, lat2, lon2) {
        const R = 6371000.0, toRad = Math.PI/180.0
        const dLat = (lat2-lat1)*toRad, dLon = (lon2-lon1)*toRad
        const a = Math.sin(dLat/2)**2 + Math.cos(lat1*toRad)*Math.cos(lat2*toRad)*Math.sin(dLon/2)**2
        return 2*R*Math.atan2(Math.sqrt(a), Math.sqrt(1-a))
    }
    function radiusMetersForZoom(zoom, lat) {
        const px = 30.0
        const mpp = 156543.03392 * Math.cos(lat * Math.PI/180.0) / Math.pow(2, zoom)
        return px * mpp
    }
    function cluster(list, zoom) {
        if (!list || list.length===0) return []
        const groups = []
        const r = radiusMetersForZoom(zoom, centerLat)
        for (let m of list) {
            let merged = false
            for (let g of groups) {
                if (haversineMeters(g.lat,g.lng,m.lat,m.lng) <= r) { g.count++; g.items.push(m); merged=true; break }
            }
            if (!merged) groups.push({lat:m.lat, lng:m.lng, count:1, items:[m]})
        }
        return groups
    }
    function updateCluster() { clustered = cluster(markers, map.zoomLevel) }
    onMarkersChanged: updateCluster()

    Map {
        id: map
        anchors.fill: parent
        // ✅ write-once 준수: 단일 바인딩
        plugin: mapPlugin
        center: QtPositioning.coordinate(root.centerLat, root.centerLng)
        zoomLevel: root.zoomLevel

        // 현재 위치 핀(옵션)
        MapQuickItem {
            coordinate: QtPositioning.coordinate(root.centerLat, root.centerLng)
            anchorPoint.x: 9; anchorPoint.y: 9
            sourceItem: Rectangle {
                width: 18; height: 18; radius: 9
                border.color:"#111"; border.width: 1.5
                color:"#e74c3c"
            }
        }

        // 단순/클러스터 마커
        Repeater {
            model: root.clustered
            MapQuickItem {
                coordinate: QtPositioning.coordinate(modelData.lat, modelData.lng)
                anchorPoint.x: 11; anchorPoint.y: 11
                sourceItem: Item {
                    width: 22; height: 22
                    Rectangle {
                        anchors.centerIn: parent
                        width: 20; height: 20; radius: 10
                        color: modelData.count > 1 ? "#1f78b4" : "#10b981"
                        border.color: "white"; border.width: 2
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: modelData.count > 1
                        text: modelData.count
                        color: "white"; font.pixelSize: 12; font.bold: true
                    }
                }
            }
        }

        onZoomLevelChanged: updateCluster()
        onCenterChanged:    updateCluster()
    }

    // 히트맵(옵션)
    property bool showHeatmap: true
    property real heatIntensity: 1.0
    Canvas {
        id: heat
        anchors.fill: map
        visible: root.showHeatmap
        antialiasing: true
        function drawHeatPoints(points) {
            var ctx = getContext("2d"); ctx.clearRect(0,0,width,height);
            var baseR = Math.max(8, 40 - map.zoomLevel * 1.5);
            for (var i=0; i<points.length; ++i) {
                var p = points[i]; var pt = map.fromCoordinate(QtPositioning.coordinate(p.lat, p.lng));
                var cnt = (p.count ? p.count : 1);
                var r = baseR * (1 + Math.log(1+cnt)/2);
                var g = ctx.createRadialGradient(pt.x, pt.y, 0, pt.x, pt.y, r);
                var alpha = Math.min(0.45*root.heatIntensity, 0.85);
                g.addColorStop(0.0, "rgba(255,0,0," + alpha + ")");
                g.addColorStop(1.0, "rgba(255,0,0,0)");
                ctx.fillStyle = g; ctx.fillRect(pt.x-r, pt.y-r, 2*r, 2*r);
            }
        }
        onPaint: drawHeatPoints(root.clustered.length ? root.clustered : root.markers)
        Connections {
            target: map
            function onZoomLevelChanged(){ heat.requestPaint() }
            function onCenterChanged(){    heat.requestPaint() }
        }
        onVisibleChanged: if (visible) requestPaint()
    }

    // 정보 배지
    Rectangle {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 12
        radius: 8
        color: "#111827"; opacity: 0.88
        height: 36
        width: infoLabel.paintedWidth + 24
        Text {
            id: infoLabel
            anchors.centerIn: parent
            text: "Ready"
            color: "white"; font.pixelSize: 14; font.bold: true
        }
    }

    // ✅ 다크/스트리트뷰 토글 버튼/코드 **삭제**
}
