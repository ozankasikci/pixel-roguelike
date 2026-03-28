#pragma once

#include "editor/EditorSceneDocument.h"

#include <memory>
#include <string>
#include <vector>

class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;

    virtual const std::string& label() const = 0;
    virtual void undo(EditorSceneDocument& document) const = 0;
    virtual void redo(EditorSceneDocument& document) const = 0;
};

class EditorDocumentStateCommand final : public IEditorCommand {
public:
    EditorDocumentStateCommand(std::string label,
                               EditorSceneDocumentState beforeState,
                               EditorSceneDocumentState afterState);

    const std::string& label() const override { return label_; }
    void undo(EditorSceneDocument& document) const override;
    void redo(EditorSceneDocument& document) const override;

private:
    std::string label_;
    EditorSceneDocumentState beforeState_;
    EditorSceneDocumentState afterState_;
};

class EditorCommandStack {
public:
    void reset(EditorSceneDocument& document);
    void markSaved(EditorSceneDocument& document);

    bool canUndo() const;
    bool canRedo() const;

    const char* undoLabel() const;
    const char* redoLabel() const;

    bool undo(EditorSceneDocument& document);
    bool redo(EditorSceneDocument& document);

    bool pushDocumentStateCommand(std::string label,
                                  const EditorSceneDocumentState& beforeState,
                                  const EditorSceneDocumentState& afterState,
                                  EditorSceneDocument& document);

private:
    struct SavedSignature {
        std::string scene;
        std::string environment;
    };

    static constexpr std::size_t kMaxCommands = 256;

    static SavedSignature makeSignature(const EditorSceneDocumentState& state);
    void syncDirtyFlags(EditorSceneDocument& document) const;
    void trimToLimit();

    std::vector<std::unique_ptr<IEditorCommand>> commands_;
    std::size_t cursor_ = 0;
    SavedSignature savedSignature_;
};
