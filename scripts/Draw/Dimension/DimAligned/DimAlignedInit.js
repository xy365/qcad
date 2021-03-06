function init(basePath) {
    var action = new RGuiAction(qsTranslate("DimAligned", "&Aligned"),
        RMainWindowQt.getMainWindow());
    action.setRequiresDocument(true);
    action.setScriptFile(basePath + "/DimAligned.js");
    action.setIcon(basePath + "/DimAligned.svg");
    action.setStatusTip(qsTranslate("DimAligned", "Draw aligned dimension"));
    action.setDefaultShortcut(new QKeySequence("d,a"));
    action.setDefaultCommands(["dimaligned", "da"]);
    action.setSortOrder(100);
    EAction.addGuiActionTo(action, Dimension, true, true, true);
}
