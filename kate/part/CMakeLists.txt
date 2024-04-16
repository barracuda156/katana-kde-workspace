# define project
project(katepart)

# these subdirs have their own CMakeLists
add_subdirectory(data)

# syntax highlighting data files
add_subdirectory(syntax/data)

# includes
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer
    ${CMAKE_CURRENT_SOURCE_DIR}/completion
    ${CMAKE_CURRENT_SOURCE_DIR}/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/document
    ${CMAKE_CURRENT_SOURCE_DIR}/mode
    ${CMAKE_CURRENT_SOURCE_DIR}/render
    ${CMAKE_CURRENT_SOURCE_DIR}/search
    ${CMAKE_CURRENT_SOURCE_DIR}/syntax
    ${CMAKE_CURRENT_SOURCE_DIR}/schema
    ${CMAKE_CURRENT_SOURCE_DIR}/undo
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/view
    ${CMAKE_CURRENT_SOURCE_DIR}/swapfile
    ${CMAKE_CURRENT_SOURCE_DIR}/variableeditor
    ${CMAKE_CURRENT_SOURCE_DIR}/kte5
)

# our sources
set(katepart_PART_SRCS
    # text buffer & buffer helpers
    buffer/katetextbuffer.cpp
    buffer/katetextblock.cpp
    buffer/katetextline.cpp
    buffer/katetextcursor.cpp
    buffer/katetextrange.cpp
    buffer/katetexthistory.cpp
    buffer/katetextfolding.cpp

    # completion (widget, model, delegate, ...)
    completion/codecompletionmodelcontrollerinterfacev4.cpp
    completion/katecompletionwidget.cpp
    completion/katecompletionmodel.cpp
    completion/katecompletiontree.cpp
    completion/katecompletionconfig.cpp
    completion/kateargumenthinttree.cpp
    completion/kateargumenthintmodel.cpp
    completion/katecompletiondelegate.cpp
    completion/expandingtree/expandingwidgetmodel.cpp
    completion/expandingtree/expandingdelegate.cpp
    completion/expandingtree/expandingtree.cpp

    # simple internal word completion
    completion/katewordcompletion.cpp
    # internal syntax-file based keyword completion
    completion/katekeywordcompletion.cpp

    # dialogs
    dialogs/katedialogs.cpp

    # document (THE document, buffer, lines/cursors/..., CORE STUFF)
    document/katedocument.cpp
    document/katebuffer.cpp

    # undo
    undo/kateundo.cpp
    undo/katemodifiedundo.cpp
    undo/kateundomanager.cpp

    # mode (modemanager and co)
    mode/katemodemanager.cpp
    mode/katemodeconfigpage.cpp
    mode/katemodemenu.cpp
    mode/katewildcardmatcher.cpp

    # modeline variable editor
    variableeditor/variablelineedit.cpp
    variableeditor/variablelistview.cpp
    variableeditor/variableeditor.cpp
    variableeditor/variableitem.cpp
    variableeditor/katehelpbutton.cpp

    # rendering stuff (katerenderer and helpers)
    render/katerenderer.cpp
    render/katerenderrange.cpp
    render/katelayoutcache.cpp
    render/katetextlayout.cpp
    render/katelinelayout.cpp

    # search stuff
    search/kateregexp.cpp
    search/kateplaintextsearch.cpp
    search/kateregexpsearch.cpp
    search/katematch.cpp
    search/katesearchbar.cpp

    # syntax related stuff (highlighting, xml file parsing, ...)
    syntax/katesyntaxmanager.cpp
    syntax/katehighlight.cpp
    syntax/katehighlighthelpers.cpp
    syntax/katehighlightmenu.cpp
    syntax/katesyntaxdocument.cpp
    syntax/kateextendedattribute.cpp

    # view stuff (THE view and its helpers)
    view/kateview.cpp
    view/kateviewinternal.cpp
    view/kateviewhelpers.cpp
    view/katemessagewidget.cpp
    view/katetextanimation.cpp

    # spell checking
    spellcheck/prefixstore.h
    spellcheck/prefixstore.cpp
    spellcheck/ontheflycheck.h
    spellcheck/ontheflycheck.cpp
    spellcheck/spellcheck.h
    spellcheck/spellcheck.cpp
    spellcheck/spellingmenu.h
    spellcheck/spellingmenu.cpp

    # generic stuff, unsorted...
    utils/katecmds.cpp
    utils/kateconfig.cpp
    utils/katebookmarks.cpp
    utils/kateautoindent.cpp
    utils/kateprinter.cpp
    utils/kateglobal.cpp
    utils/katecmd.cpp
    utils/katepartpluginmanager.cpp
    utils/katedefaultcolors.cpp

    # schema
    schema/kateschema.cpp
    schema/kateschemaconfig.cpp
    schema/katestyletreewidget.cpp
    schema/katecolortreewidget.cpp
    schema/katecategorydrawer.cpp

    # swapfile
    swapfile/kateswapdiffcreator.cpp
    swapfile/kateswapfile.cpp

    # KDE5: move to KTextEditor
    kte5/documentcursor.cpp
)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=13000)

add_library(katepartinterfaces SHARED ${katepart_PART_SRCS})

target_link_libraries(katepartinterfaces
    KDE4::kdecore
    KDE4::kparts
    KDE4::kcmutils
    KDE4::ktexteditor
)

set_target_properties(
    katepartinterfaces PROPERTIES
    VERSION ${GENERIC_LIB_VERSION}
    SOVERSION ${GENERIC_LIB_SOVERSION}
)

generate_export_header(katepartinterfaces)

# install kate part interfaces
install(
    TARGETS katepartinterfaces
    DESTINATION ${KDE4_LIB_INSTALL_DIR}
)

# kate part itself just is interfaces + the factory
kde4_add_plugin(katepart utils/katefactory.cpp)

# link the part, use kate part interfaces + kde stuff
target_link_libraries(katepart
    KDE4::kdecore
    KDE4::kparts
    KDE4::kcmutils
    KDE4::ktexteditor
    katepartinterfaces
)

# install the part
install(
    TARGETS katepart
    DESTINATION ${KDE4_PLUGIN_INSTALL_DIR}
)
