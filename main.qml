import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import QtQuick.Dialogs 1.3

ApplicationWindow {
    id: root
    width: 1000
    height: 720
    visible: true
    title: ai.agentMode ? qsTr("AiChat Agent - DeepSeek Coding Agent")
                         : qsTr("AiChat - DeepSeek AI Assistant")

    Component.onCompleted: {
        ai.loadConfig()
        refreshChatList()
    }

    // ===== Color scheme =====
    readonly property color accentColor: "#6366f1"
    readonly property color agentColor: "#8b5cf6"
    readonly property color bgColor: "#f8fafc"
    readonly property color cardColor: "#ffffff"
    readonly property color textColor: "#1e293b"
    readonly property color subColor: "#64748b"
    readonly property color borderColor: "#e2e8f0"
    readonly property color errorColor: "#ef4444"
    readonly property color successColor: "#10b981"
    readonly property color warnColor: "#f59e0b"
    readonly property color fileTagColor: "#3b82f6"

    // ===== State =====
    property var attachedFiles: []
    property var chatHistoryModel: []
    property string pendingChatPath: ""

    // ===== Helper functions =====
    function actionIcon(type) {
        if (type === "list_project_files" || type === "list_files") return "DIR"
        if (type === "read_file") return "READ"
        if (type === "create_file") return "CREATE"
        if (type === "modify_file") return "MODIFY"
        if (type === "delete_file") return "DEL"
        if (type === "run_command") return "CMD"
        return type.toUpperCase()
    }

    function actionColor(type) {
        if (type === "create_file") return successColor
        if (type === "modify_file") return warnColor
        if (type === "delete_file") return errorColor
        if (type === "run_command") return agentColor
        if (type === "read_file") return "#3b82f6"
        return subColor
    }

    function statusColor(status) {
        if (status === "done") return successColor
        if (status === "error") return errorColor
        if (status === "rejected") return subColor
        if (status === "running") return warnColor
        return subColor
    }

    function refreshChatList() {
        chatHistoryModel = ai.chatList()
    }

    function formatTime(isoStr) {
        if (!isoStr || isoStr === "") return ""
        var dt = new Date(isoStr)
        if (isNaN(dt.getTime())) return isoStr
        return dt.toLocaleDateString() + " " + dt.toLocaleTimeString([], "short")
    }

    // ===== @file handling =====
    function handleAtTrigger(text, cursorPos) {
        var beforeCursor = text.substring(0, cursorPos)
        var atIndex = beforeCursor.lastIndexOf("@")

        if (atIndex < 0) {
            fileDropdown.close()
            return
        }

        if (atIndex > 0) {
            var prev = beforeCursor[atIndex - 1]
            if (prev !== " " && prev !== "\n" && prev !== "\t") {
                fileDropdown.close()
                return
            }
        }

        var prefix = beforeCursor.substring(atIndex + 1)
        if (prefix.length > 0 && prefix.indexOf(" ") >= 0) {
            fileDropdown.close()
            return
        }

        if (ai.projectDir === "") {
            fileDropdown.close()
            return
        }

        fileDropdown.atIndex = atIndex
        fileDropdown.prefix = prefix
        fileDropdown.fileModel = ai.searchProjectFiles(prefix)
        if (fileDropdown.fileModel.length > 0) {
            if (!fileDropdown.opened) fileDropdown.open()
        } else {
            fileDropdown.close()
        }
    }

    function selectFile(filePath, fileName) {
        for (var i = 0; i < attachedFiles.length; i++) {
            if (attachedFiles[i] === filePath) {
                toast.showMsg(qsTr("File already attached"))
                fileDropdown.close()
                return
            }
        }

        var newFiles = attachedFiles.slice()
        newFiles.push(filePath)
        attachedFiles = newFiles

        var text = inputField.text
        var before = text.substring(0, fileDropdown.atIndex)
        var after = text.substring(inputField.cursorPosition)
        inputField.text = before + after
        inputField.cursorPosition = before.length

        fileDropdown.close()
        toast.showMsg(qsTr("Attached: ") + fileName)
    }

    function removeAttachedFile(index) {
        var newFiles = []
        for (var i = 0; i < attachedFiles.length; i++) {
            if (i !== index) newFiles.push(attachedFiles[i])
        }
        attachedFiles = newFiles
    }

    function doSend() {
        if (ai.loading) {
            ai.stopGeneration()
            return
        }
        var text = inputField.text.trim()
        if (text === "" && attachedFiles.length === 0) return

        if (attachedFiles.length > 0) {
            ai.sendMessageWithFiles(text, attachedFiles)
        } else {
            ai.sendMessage(text)
        }
        inputField.text = ""
        attachedFiles = []
    }

    // ===== Toast =====
    Label {
        id: toast
        property bool show: false
        text: ""
        anchors.bottom: inputBar.top
        anchors.bottomMargin: show ? 12 : -40
        anchors.horizontalCenter: parent.horizontalCenter
        padding: 8
        background: Rectangle { color: "#1e293b"; radius: 6 }
        color: "white"
        font.pixelSize: 12
        opacity: show ? 1 : 0
        z: 100
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Behavior on anchors.bottomMargin { NumberAnimation { duration: 200 } }
        Timer { id: toastTimer; interval: 2000; onTriggered: toast.show = false }
        function showMsg(msg) { text = msg; show = true; toastTimer.restart() }
    }

    // ===== Folder Dialog =====
    FileDialog {
        id: folderDialog
        title: qsTr("Select Project Directory")
        selectFolder: true
        selectExisting: true
        property bool forChatLoad: false
        onAccepted: {
            var path = folderDialog.toString().replace("file:///", "").replace(/\//g, "/")
            if (forChatLoad) {
                ai.setProjectDirForLoadedChat(path)
                forChatLoad = false
            } else {
                ai.setProjectDir(path)
            }
        }
    }

    // ===== Main layout =====
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ========== Top bar ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: cardColor
            z: 10

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: 8

                Rectangle {
                    width: 8; height: 8
                    radius: 4
                    color: ai.loading ? warnColor :
                           (ai.apiKey !== "" ? successColor : errorColor)
                }

                Text {
                    text: ai.agentMode ? "AiChat Agent" : "AiChat"
                    font.pixelSize: 14
                    font.bold: true
                    color: ai.agentMode ? agentColor : textColor
                }

                Text {
                    text: ai.hasPendingAction ?
                          qsTr("Waiting: ") + ai.pendingAction.description :
                          (ai.statusText || qsTr("Ready"))
                    font.pixelSize: 11
                    color: ai.hasPendingAction ? warnColor : subColor
                    elide: Text.ElideRight
                    Layout.maximumWidth: 200
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("History")
                    flat: true
                    font.pixelSize: 11
                    onClicked: {
                        refreshChatList()
                        chatHistoryPopup.open()
                    }
                }

                Button {
                    text: qsTr("New")
                    flat: true
                    font.pixelSize: 11
                    onClicked: {
                        ai.newChat()
                        toast.showMsg(qsTr("New chat started"))
                    }
                }

                Button {
                    text: ai.projectDir !== "" ?
                          ai.projectDir.substring(Math.max(0, ai.projectDir.lastIndexOf("/") + 1)) :
                          qsTr("Select Dir")
                    flat: true
                    font.pixelSize: 11
                    onClicked: {
                        folderDialog.forChatLoad = false
                        folderDialog.open()
                    }
                }

                Switch {
                    text: qsTr("Agent")
                    checked: ai.agentMode
                    onToggled: {
                        ai.setAgentMode(checked)
                        if (checked && ai.projectDir === "") {
                            folderDialog.forChatLoad = false
                            folderDialog.open()
                        }
                    }
                }

                Button {
                    text: qsTr("Settings")
                    flat: true
                    onClicked: settingsDrawer.open()
                }

                Button {
                    text: qsTr("Clear")
                    flat: true
                    onClicked: ai.clearHistory()
                }
            }
        }

        // ========== Quick prompts ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: cardColor

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: borderColor
            }

            ScrollView {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Repeater {
                        model: ai.quickPrompts
                        Button {
                            text: modelData.title
                            flat: true
                            font.pixelSize: 11
                            padding: 4
                            onClicked: {
                                inputField.text = modelData.prompt
                                inputField.forceActiveFocus()
                            }
                        }
                    }
                }
            }
        }

        // ========== Chat area ==========
        ScrollView {
            id: chatScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            Column {
                width: chatScroll.width
                spacing: 8
                topPadding: 12
                bottomPadding: 12
                leftPadding: 16
                rightPadding: 16

                Repeater {
                    model: ai.messageHistory

                    Rectangle {
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: bubbleColumn.implicitHeight + 16
                        color: modelData.isUser ? accentColor : cardColor
                        radius: 8
                        border.color: modelData.isError ? errorColor : borderColor
                        border.width: modelData.isError || !modelData.isUser ? 1 : 0

                        ColumnLayout {
                            id: bubbleColumn
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            Text {
                                text: modelData.isUser ? "You" : (modelData.isError ? "Error" : "AI")
                                font.pixelSize: 10
                                font.bold: true
                                color: modelData.isUser ? "#ffffff80" :
                                       (modelData.isError ? errorColor : subColor)
                                Layout.fillWidth: true
                            }

                            Text {
                                text: modelData.content
                                font.pixelSize: 13
                                color: modelData.isUser ? "white" :
                                       (modelData.isError ? errorColor : textColor)
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                lineHeight: 1.4
                            }

                            Text {
                                text: qsTr("Copy")
                                font.pixelSize: 10
                                color: modelData.isUser ? "#ffffff60" : subColor
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        ai.copyToClipboard(modelData.content)
                                        toast.showMsg(qsTr("Copied to clipboard"))
                                    }
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: {
                                contextMenu.content = modelData.content
                                contextMenu.popup()
                            }
                        }
                    }
                }

                Item {
                    width: parent.width - 32
                    height: ai.loading && ai.response === "" ? 30 : 0
                    visible: ai.loading && ai.response === ""
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "..."
                        font.pixelSize: 18
                        color: subColor
                    }
                }
            }
        }

        // ========== Action log panel ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: ai.agentMode && ai.actionLog.length > 0 ? 120 : 0
            visible: ai.agentMode && ai.actionLog.length > 0
            color: "#f8fafc"
            clip: true

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: borderColor
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Text {
                    text: qsTr("Action Log (%1)").arg(ai.actionLog.length)
                    font.pixelSize: 11
                    font.bold: true
                    color: subColor
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    Column {
                        width: parent.width
                        spacing: 2

                        Repeater {
                            model: ai.actionLog

                            RowLayout {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: actionIcon(modelData.type)
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: actionColor(modelData.type)
                                    Layout.preferredWidth: 50
                                }

                                Text {
                                    text: modelData.description
                                    font.pixelSize: 10
                                    color: textColor
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: modelData.status
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: statusColor(modelData.status)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ========== Pending action panel ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: ai.hasPendingAction ? 100 : 0
            visible: ai.hasPendingAction
            color: "#fffbeb"
            clip: true

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: warnColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: ai.pendingAction.type ? actionIcon(ai.pendingAction.type) + ": " + ai.pendingAction.description : ""
                        font.pixelSize: 12
                        font.bold: true
                        color: textColor
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }

                    Text {
                        text: ai.pendingAction.detail || ""
                        font.pixelSize: 10
                        color: subColor
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }
                }

                Button {
                    text: qsTr("Reject")
                    onClicked: ai.rejectPendingAction()
                }

                Button {
                    text: qsTr("Approve")
                    highlighted: true
                    onClicked: ai.approvePendingAction()
                }
            }
        }

        // ========== Attached files bar ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: attachedFiles.length > 0 ? 36 : 0
            visible: attachedFiles.length > 0
            color: "#f0f7ff"
            clip: true

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: fileTagColor
            }

            ScrollView {
                anchors.fill: parent
                anchors.margins: 4
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Repeater {
                        model: attachedFiles

                        Rectangle {
                            height: 28
                            width: fileTagRow.implicitWidth + 12
                            color: "#dbeafe"
                            radius: 4
                            border.color: fileTagColor
                            border.width: 1

                            RowLayout {
                                id: fileTagRow
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    text: modelData.substring(Math.max(0, modelData.lastIndexOf("/") + 1))
                                    font.pixelSize: 11
                                    color: fileTagColor
                                }

                                Text {
                                    text: "\u00d7"
                                    font.pixelSize: 14
                                    color: errorColor
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: removeAttachedFile(index)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ========== Input bar ==========
        Rectangle {
            id: inputBar
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: cardColor

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: ai.projectDir !== "" ? "@" : ""
                    font.pixelSize: 16
                    color: fileTagColor
                    visible: ai.projectDir !== ""
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            inputField.text += "@"
                            inputField.cursorPosition = inputField.text.length
                            handleAtTrigger(inputField.text, inputField.cursorPosition)
                        }
                    }
                }

                TextField {
                    id: inputField
                    Layout.fillWidth: true
                    placeholderText: ai.projectDir !== "" ?
                        qsTr("Type @ to mention a file...") :
                        qsTr("Type a message...")
                    font.pixelSize: 13
                    selectByMouse: true
                    onAccepted: doSend()
                    Keys.onReturnPressed: doSend()

                    onTextChanged: {
                        handleAtTrigger(text, cursorPosition)
                    }

                    Keys.onEscapePressed: {
                        fileDropdown.close()
                    }
                }

                Button {
                    id: sendButton
                    text: ai.loading ? qsTr("Stop") : qsTr("Send")
                    highlighted: true
                    onClicked: doSend()
                }
            }
        }

        // ========== Permission level indicator ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            color: "#f1f5f9"

            Text {
                anchors.centerIn: parent
                text: {
                    if (ai.permissionLevel === 0) return qsTr("Permission: Low (ask for all)")
                    if (ai.permissionLevel === 1) return qsTr("Permission: Medium (ask for delete/cmd)")
                    return qsTr("Permission: High (no ask)")
                }
                font.pixelSize: 10
                color: subColor
            }
        }
    }

    // ===== File dropdown popup =====
    Popup {
        id: fileDropdown
        property string prefix: ""
        property int atIndex: 0
        property var fileModel: []
        x: inputBar.x + 28
        y: inputBar.y - 220
        width: 320
        height: Math.min(fileModel.length * 32 + 8, 200)
        modal: false
        focus: true
        clip: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Rectangle {
            anchors.fill: parent
            color: cardColor
            border.color: borderColor
            border.width: 1
            radius: 6

            ListView {
                id: fileListView
                anchors.fill: parent
                anchors.margins: 4
                model: fileDropdown.fileModel
                spacing: 2
                clip: true

                delegate: Rectangle {
                    width: fileListView.width
                    height: 28
                    color: mouseArea.containsMouse ? "#e0e7ff" : "transparent"
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: modelData.name
                            font.pixelSize: 12
                            font.bold: true
                            color: textColor
                        }

                        Text {
                            text: modelData.dir !== "." ? modelData.dir : ""
                            font.pixelSize: 10
                            color: subColor
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: selectFile(modelData.path, modelData.name)
                    }
                }
            }
        }

        onClosed: {
            prefix = ""
            fileModel = []
        }
    }

    // ===== Chat history popup =====
    Popup {
        id: chatHistoryPopup
        x: root.width / 2 - 200
        y: 60
        width: 400
        height: 450
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Rectangle {
            anchors.fill: parent
            color: cardColor
            border.color: borderColor
            border.width: 1
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Chat History")
                        font.pixelSize: 16
                        font.bold: true
                        color: textColor
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("New Chat")
                        flat: true
                        font.pixelSize: 11
                        onClicked: {
                            ai.newChat()
                            chatHistoryPopup.close()
                            toast.showMsg(qsTr("New chat started"))
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: borderColor }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ListView {
                        id: chatListView
                        anchors.fill: parent
                        model: chatHistoryModel
                        spacing: 4
                        clip: true

                        delegate: Rectangle {
                            width: chatListView.width
                            height: 72
                            color: delegateMouse.containsMouse ? "#f0f4ff" : "transparent"
                            border.color: borderColor
                            border.width: 1
                            radius: 6

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: modelData.title
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: textColor
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: modelData.agentMode ? "Agent" : "Chat"
                                        font.pixelSize: 9
                                        color: modelData.agentMode ? agentColor : accentColor
                                    }
                                }

                                Text {
                                    text: modelData.projectDir
                                    font.pixelSize: 9
                                    color: subColor
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        text: modelData.messageCount + qsTr(" messages")
                                        font.pixelSize: 9
                                        color: subColor
                                    }

                                    Text {
                                        text: formatTime(modelData.updatedAt)
                                        font.pixelSize: 9
                                        color: subColor
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: qsTr("Delete")
                                        font.pixelSize: 9
                                        color: errorColor
                                        visible: delegateMouse.containsMouse
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                ai.deleteChat(modelData.id)
                                                refreshChatList()
                                                toast.showMsg(qsTr("Chat deleted"))
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: delegateMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    ai.loadChat(modelData.id)
                                    chatHistoryPopup.close()
                                }
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: chatHistoryModel.length === 0
                        text: qsTr("No saved chats")
                        font.pixelSize: 14
                        color: subColor
                    }
                }

                Button {
                    text: qsTr("Close")
                    Layout.fillWidth: true
                    onClicked: chatHistoryPopup.close()
                }
            }
        }
    }

    // ===== Path not found dialog =====
    Popup {
        id: pathNotFoundDialog
        x: root.width / 2 - 200
        y: root.height / 2 - 80
        width: 400
        height: 180
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose

        Rectangle {
            anchors.fill: parent
            color: cardColor
            border.color: errorColor
            border.width: 2
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: qsTr("Project Path Not Found")
                    font.pixelSize: 16
                    font.bold: true
                    color: errorColor
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("The project directory from this chat does not exist:\n") +
                          pendingChatPath +
                          qsTr("\n\nPlease select a new project directory.")
                    font.pixelSize: 12
                    color: textColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("Select Path")
                        highlighted: true
                        onClicked: {
                            folderDialog.forChatLoad = true
                            folderDialog.open()
                        }
                    }
                }
            }
        }
    }

    // ===== Keep history dialog =====
    Popup {
        id: keepHistoryDialog
        x: root.width / 2 - 200
        y: root.height / 2 - 80
        width: 400
        height: 160
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose

        property int msgCount: 0
        property string chatTitle: ""

        Rectangle {
            anchors.fill: parent
            color: cardColor
            border.color: warnColor
            border.width: 2
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: qsTr("Keep Chat History?")
                    font.pixelSize: 16
                    font.bold: true
                    color: textColor
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("This chat has ") + keepHistoryDialog.msgCount +
                          qsTr(" messages from \"") + keepHistoryDialog.chatTitle +
                          qsTr("\".\nKeep the history? AI will reference previous messages if kept.")
                    font.pixelSize: 12
                    color: subColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("Don't Keep")
                        onClicked: {
                            ai.keepLoadedHistory(false)
                            keepHistoryDialog.close()
                            toast.showMsg(qsTr("Started fresh chat"))
                        }
                    }

                    Button {
                        text: qsTr("Keep")
                        highlighted: true
                        onClicked: {
                            ai.keepLoadedHistory(true)
                            keepHistoryDialog.close()
                            toast.showMsg(qsTr("History kept"))
                        }
                    }
                }
            }
        }
    }

    // ===== Connections to AiController signals =====
    Connections {
        target: ai

        onProjectDirNotFound: {
            pendingChatPath = oldPath
            pathNotFoundDialog.open()
        }

        onAskKeepHistory: {
            keepHistoryDialog.msgCount = messageCount
            keepHistoryDialog.chatTitle = title
            keepHistoryDialog.open()
        }

        onChatLoaded: {
            toast.showMsg(qsTr("Loaded: ") + title)
        }
    }

    // ===== Context menu =====
    Menu {
        id: contextMenu
        property string content: ""

        MenuItem {
            text: qsTr("Copy")
            onTriggered: {
                ai.copyToClipboard(contextMenu.content)
                toast.showMsg(qsTr("Copied"))
            }
        }
    }

    // ===== Settings drawer =====
    Drawer {
        id: settingsDrawer
        width: 360
        height: root.height
        edge: Qt.RightEdge

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Text {
                text: qsTr("Settings")
                font.pixelSize: 18
                font.bold: true
                color: textColor
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: borderColor }

            Label { text: qsTr("API Key"); font.pixelSize: 12; color: subColor }
            TextField {
                id: apiKeyField
                Layout.fillWidth: true
                placeholderText: qsTr("Enter DeepSeek API Key")
                text: ai.apiKey
                font.pixelSize: 12
                selectByMouse: true
                echoMode: TextInput.Password
            }

            Label { text: qsTr("Model"); font.pixelSize: 12; color: subColor }
            TextField {
                id: modelField
                Layout.fillWidth: true
                placeholderText: "deepseek-chat"
                text: ai.model
                font.pixelSize: 12
                selectByMouse: true
            }

            Label { text: qsTr("API URL"); font.pixelSize: 12; color: subColor }
            TextField {
                id: apiUrlField
                Layout.fillWidth: true
                placeholderText: "https://api.deepseek.com/v1/chat/completions"
                text: ai.apiUrl
                font.pixelSize: 12
                selectByMouse: true
            }

            Label { text: qsTr("AI Role"); font.pixelSize: 12; color: subColor }
            ComboBox {
                id: roleCombo
                Layout.fillWidth: true
                model: promptManager.roleNames()
                font.pixelSize: 12
            }

            Label { text: qsTr("Project Directory"); font.pixelSize: 12; color: subColor }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: projectDirField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Select project directory")
                    text: ai.projectDir
                    font.pixelSize: 12
                    selectByMouse: true
                    readOnly: true
                }

                Button {
                    text: qsTr("Browse")
                    onClicked: {
                        folderDialog.forChatLoad = false
                        folderDialog.open()
                    }
                }
            }

            Label { text: qsTr("Permission Level"); font.pixelSize: 12; color: subColor }
            ComboBox {
                id: permLevelCombo
                Layout.fillWidth: true
                model: ListModel {
                    ListElement { text: qsTr("Low - Ask for all actions") }
                    ListElement { text: qsTr("Medium - Ask for delete/command") }
                    ListElement { text: qsTr("High - No confirmation needed") }
                }
                font.pixelSize: 12
                currentIndex: ai.permissionLevel
            }

            Item { Layout.fillHeight: true }

            Button {
                text: qsTr("Save")
                highlighted: true
                Layout.fillWidth: true
                onClicked: {
                    ai.setApiKey(apiKeyField.text)
                    ai.setModel(modelField.text)
                    ai.setApiUrl(apiUrlField.text)
                    ai.setProjectDir(projectDirField.text)
                    ai.setPermissionLevel(permLevelCombo.currentIndex)

                    var role = roleCombo.currentIndex
                    var prompt = promptManager.getSystemPrompt(role)
                    ai.setSystemPrompt(prompt)

                    ai.saveConfig(apiKeyField.text, modelField.text, role)

                    toast.showMsg(qsTr("Settings saved"))
                    settingsDrawer.close()
                }
            }
        }
    }
}
