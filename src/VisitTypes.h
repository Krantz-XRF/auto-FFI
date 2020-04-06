#pragma once

#include <optional>

#include <clang/AST/AST.h>

#include "Config.h"
#include "Module.h"
#include "Tag.h"
#include "Types.h"

namespace ffi
{
class Visitor
{
  public:
    Visitor(config::Config& cfg, clang::ASTContext& context, bool header_group)
        : header_group{header_group}, cfg{cfg}, context{context}
    {
    }

    bool checkDecl(const clang::Decl* decl) const noexcept;

    void matchTranslationUnit(const clang::TranslationUnitDecl& decl,
                              ModuleContents& mod) const noexcept;
    [[nodiscard]] std::optional<ffi::Entity> matchEntity(
        const clang::Decl& decl) const noexcept;
    [[nodiscard]] std::optional<ffi::TagDecl> matchTag(
        const clang::Decl& decl) const noexcept;
    [[nodiscard]] std::optional<Type> matchType(const clang::NamedDecl& decl,
                                                const clang::Type& type) const
        noexcept;
    [[nodiscard]] std::optional<Entity> matchVarRaw(
        const clang::VarDecl& decl) const noexcept;
    [[nodiscard]] std::optional<Entity> matchVar(
        const clang::VarDecl& decl) const noexcept;
    [[nodiscard]] std::optional<Entity> matchParam(
        const clang::ParmVarDecl& param) const noexcept;
    [[nodiscard]] std::optional<Entity> matchFunction(
        const clang::FunctionDecl& decl) const noexcept;
    [[nodiscard]] std::optional<TagDecl> matchEnum(
        const clang::EnumDecl& decl, llvm::StringRef defName = {}) const
        noexcept;
    [[nodiscard]] std::optional<TagDecl> matchStruct(
        const clang::RecordDecl& decl, llvm::StringRef defName = {}) const
        noexcept;
    [[nodiscard]] std::optional<TagDecl> matchTypedef(
        const clang::TypedefNameDecl& decl) const noexcept;

  private:
    bool header_group;
    config::Config& cfg;
    clang::ASTContext& context;
};

inline uintptr_t getUniqueID(const clang::Decl& decl);
} // namespace ffi
