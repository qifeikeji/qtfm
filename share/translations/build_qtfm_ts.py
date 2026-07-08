#!/usr/bin/env python3
# Generate share/translations/qtfm_zh_CN.ts from qtfm_strings.json
import json
import os
import xml.sax.saxutils as xu

DIR = os.path.dirname(os.path.abspath(__file__))
JSON_PATH = os.path.join(DIR, "qtfm_strings.json")
TS_PATH = os.path.join(DIR, "qtfm_zh_CN.ts")

# Exact overrides (preferred). Keys are English source strings.
EXACT = {
    "Settings": "设置",
    "General": "常规",
    "Appearance": "外观",
    "Custom Actions": "自定义操作",
    "Shortcuts": "快捷键",
    "Open with": "打开方式",
    "Mime Types": "MIME 类型",
    "Advanced": "高级",
    "Behaviour": "行为",
    "Confirmation": "确认",
    "Terminal emulator": "终端模拟器",
    "Command: ": "命令：",
    "Drag and Drop Default action: ": "拖放默认操作：",
    "Drag and Drop CTRL action: ": "按住 Ctrl 拖放：",
    "Drag and Drop SHIFT action: ": "按住 Shift 拖放：",
    "Drag and Drop ALT action: ": "按住 Alt 拖放：",
    "Enable Single Click": "启用单击打开",
    "Enable path history": "启用路径历史",
    "Ask before file is deleted: ": "删除文件前询问：",
    "Ask": "询问",
    "Copy": "复制",
    "Move": "移动",
    "Link": "链接",
    "No": "否",
    "Directories only": "仅文件夹",
    "Everything": "全部",
    "Use \"Dark Mode\"": "使用深色模式",
    "Colors on file names": "文件名着色",
    "Show path in window title": "在窗口标题显示路径",
    "Show Home button": "显示“主页”按钮",
    "Show \"new tab\" button": "显示“新建标签”按钮",
    "Show Terminal button": "显示“终端”按钮",
    "Icon view spacing": "图标视图间距",
    "Icon view size": "图标视图大小",
    "List row height": "列表行高",
    "Bookmark group tab size": "书签分组标签大小",
    "Folders always first (list)": "列表中文件夹始终在前",
    "List column: Name": "列表列：名称",
    "List column: Size": "列表列：大小",
    "List column: Date": "列表列：日期",
    "List column: Format": "列表列：格式",
    "List column: Folder": "列表列：类型",
    " px": " 像素",
    "Language": "界面语言",
    "System default": "跟随系统",
    "English": "English",
    "简体中文": "简体中文",
    "Restart to apply settings": "需要重启以应用设置",
    "You must restart application to apply language settings.": "更改界面语言后需要重启应用程序。",
    "You must restart application to apply theme settings": "更改主题后需要重启应用程序。",
    "New bookmark group": "新建书签分组",
    "Set group icon…": "设置分组图标…",
    "Delete group…": "删除分组…",
    "Delete bookmark group": "删除书签分组",
    "At least one bookmark group must remain.": "至少需要保留一个书签分组。",
    "Delete group \"%1\" and all bookmarks in it?": "删除分组“%1”及其中的所有书签？",
    "Bookmarks": "书签",
    "Disks": "磁盘",
    "Places": "位置",
    "Tree": "目录树",
    "Back": "后退",
    "Up": "上级",
    "Home": "主页",
    "New folder": "新建文件夹",
    "New file": "新建文件",
    "New Markdown file": "新建 Markdown 文件",
    "New text file": "新建文本文件",
    "Create a new folder": "创建新文件夹",
    "Create a new file": "创建新文件",
    "Create a new Markdown (.md) file": "创建新的 Markdown（.md）文件",
    "Create a new text (.txt) file": "创建新的文本（.txt）文件",
    "Delete": "删除",
    "Rename": "重命名",
    "Properties": "属性",
    "Refresh": "刷新",
    "Cut": "剪切",
    "Paste": "粘贴",
    "Open": "打开",
    "Run": "运行",
    "Terminal": "终端",
    "Preferences": "首选项",
    "Exit": "退出",
    "About %1": "关于 %1",
    "About Qt": "关于 Qt",
    "Select...": "选择…",
    "Open with": "打开方式",
    "Add bookmark": "添加书签",
    "Add separator": "添加分隔线",
    "Remove separator": "移除分隔线",
    "Remove this separator line?": "移除此分隔线？",
    "Remove this separator line": "移除此分隔线",
    "Edit": "编辑",
    "View": "视图",
    "Clear cache": "清除缓存",
    "Tabs on top": "标签栏在顶部",
    "Show thumbs": "显示缩略图",
    "Remove bookmark": "移除书签",
    "Remove the selected bookmark(s)?": "移除选中的书签？",
    "Sort by Name": "按名称排序",
    "Sort by Size": "按大小排序",
    "Sort by Date": "按日期排序",
    "Ascending": "升序",
    "Descending": "降序",
    "Hidden files": "隐藏文件",
    "Icon view": "图标视图",
    "List view": "列表视图",
    "Thumbnails": "缩略图",
    "Close tab": "关闭标签",
    "New tab": "新建标签",
    "Open in new window": "在新窗口中打开",
    "Open in new tab": "在新标签中打开",
    "Folder": "文件夹",
    "File": "文件",
    "Name": "名称",
    "Size": "大小",
    "Date Modified": "修改日期",
    "Format": "格式",
    "Storage": "存储",
    "Cancel": "取消",
    "Yes": "是",
    "No to All": "全部否",
    "Yes to All": "全部是",
    "Abort": "中止",
    "Retry": "重试",
    "Ignore": "忽略",
    "Close": "关闭",
    "Select file action": "选择文件操作",
    "What do you want to do?": "您要执行什么操作？",
    "Move here": "移动到此处",
    "Copy here": "复制到此处",
    "Link here": "链接到此处",
    "Source and destination is on a different storage.": "源与目标位于不同的存储设备。",
    "The current directory is not writable, unable to create new file.": "当前目录不可写，无法创建新文件。",
    "The current directory is not writable, unable to create new folder.": "当前目录不可写，无法创建新文件夹。",
    "Select application": "选择应用程序",
    "Application": "应用程序",
    "Launcher: ": "启动命令：",
    "<p>打开 <b>设置 (Settings) → Open with</b>。这里的规则优先于系统默认关联。</p>":
        "<p>打开 <b>设置 → 打开方式</b>。此处的规则优先于系统默认关联。</p>",
}

# Longest-first phrase replacements for strings not in EXACT.
PHRASES = [
    ("New bookmark group", "新建书签分组"),
    ("bookmark group", "书签分组"),
    ("Custom action", "自定义操作"),
    ("custom action", "自定义操作"),
    ("Open with", "打开方式"),
    ("Mime Types", "MIME 类型"),
    ("Drag and Drop", "拖放"),
    ("Terminal emulator", "终端模拟器"),
    ("Single Click", "单击"),
    ("path history", "路径历史"),
    ("window title", "窗口标题"),
    ("Icon view", "图标视图"),
    ("List view", "列表视图"),
    ("List column", "列表列"),
    ("Dark Mode", "深色模式"),
    ("file names", "文件名"),
    ("New folder", "新建文件夹"),
    ("New file", "新建文件"),
    ("New Markdown", "新建 Markdown"),
    ("New text file", "新建文本文件"),
    ("Delete group", "删除分组"),
    ("Remove bookmark", "移除书签"),
    ("Add bookmark", "添加书签"),
    ("Add separator", "添加分隔线"),
    ("Close tab", "关闭标签"),
    ("new tab", "新建标签"),
    ("new window", "新窗口"),
    ("Hidden files", "隐藏文件"),
    ("Sort by", "排序："),
    ("Ascending", "升序"),
    ("Descending", "降序"),
    ("Properties", "属性"),
    ("Preferences", "首选项"),
    ("Refresh", "刷新"),
    ("Terminal", "终端"),
    ("Bookmarks", "书签"),
    ("Settings", "设置"),
    ("General", "常规"),
    ("Appearance", "外观"),
    ("Advanced", "高级"),
    ("Shortcuts", "快捷键"),
    ("Confirmation", "确认"),
    ("Behaviour", "行为"),
    ("Command", "命令"),
    ("Copy", "复制"),
    ("Move", "移动"),
    ("Link", "链接"),
    ("Paste", "粘贴"),
    ("Cut", "剪切"),
    ("Rename", "重命名"),
    ("Delete", "删除"),
    ("Open", "打开"),
    ("Run", "运行"),
    ("Cancel", "取消"),
    ("Close", "关闭"),
    ("Select", "选择"),
    ("Folder", "文件夹"),
    ("Folders", "文件夹"),
    ("File", "文件"),
    ("Files", "文件"),
    ("Name", "名称"),
    ("Size", "大小"),
    ("Format", "格式"),
    ("Application", "应用程序"),
    ("Action", "操作"),
    ("Icon", "图标"),
    ("Path", "路径"),
    ("Search", "搜索"),
    ("Filter", "筛选"),
    ("Warning", "警告"),
    ("Error", "错误"),
    ("Completed", "已完成"),
    ("remaining", "剩余"),
    ("directories", "目录"),
    ("directory", "目录"),
    ("writable", "可写"),
    ("deleted", "已删除"),
    ("Remove", "移除"),
    ("Add", "添加"),
    ("Show", "显示"),
    ("Enable", "启用"),
    ("Disable", "禁用"),
    ("Ask before", "操作前询问"),
    ("Ask", "询问"),
    ("Storage", "存储"),
    ("Computer", "计算机"),
    ("Address", "地址"),
    ("Places", "位置"),
    ("Tree", "目录树"),
    ("Disks", "磁盘"),
    ("Home", "主页"),
    ("Back", "后退"),
    ("Up", "上级"),
    ("About", "关于"),
    ("Help", "帮助"),
    ("Save", "保存"),
    ("Load", "加载"),
    ("Default", "默认"),
    ("Custom", "自定义"),
    ("Module", "模块"),
    ("Command", "命令"),
    ("Shell", "Shell"),
    ("Output", "输出"),
    ("Capture", "捕获"),
    ("File type", "文件类型"),
    ("Launcher", "启动器"),
    ("Abort", "中止"),
    ("Retry", "重试"),
    ("Ignore", "忽略"),
    ("Yes", "是"),
    ("No", "否"),
]


def translate_source(source: str) -> str:
    if source in EXACT:
        return EXACT[source]
    if any("\u4e00" <= c <= "\u9fff" for c in source):
        return source
    out = source
    for en, zh in sorted(PHRASES, key=lambda p: -len(p[0])):
        out = out.replace(en, zh)
    return out


def main():
    with open(JSON_PATH, encoding="utf-8") as f:
        items = json.load(f)

    by_context = {}
    for item in items:
        ctx = item["context"]
        src = item["source"]
        by_context.setdefault(ctx, [])
        if src not in [x[0] for x in by_context[ctx]]:
            by_context[ctx].append((src, translate_source(src)))

    lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<!DOCTYPE TS>',
        '<TS version="2.1" language="zh_CN">',
    ]
    for ctx in sorted(by_context.keys()):
        lines.append(f"  <context>")
        lines.append(f"    <name>{xu.escape(ctx)}</name>")
        for src, tr in sorted(by_context[ctx], key=lambda x: x[0]):
            lines.append("    <message>")
            lines.append(f"      <source>{xu.escape(src)}</source>")
            lines.append(f"      <translation>{xu.escape(tr)}</translation>")
            lines.append("    </message>")
        lines.append("  </context>")
    lines.append("</TS>")

    with open(TS_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("Wrote", TS_PATH, "contexts:", len(by_context))


if __name__ == "__main__":
    main()
