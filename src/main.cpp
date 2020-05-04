﻿/* This file is part of auto-FFI (https://github.com/Krantz-XRF/auto-FFI).
 * Copyright (C) 2020 Xie Ruifeng
 *
 * auto-FFI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * auto-FFI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with auto-FFI.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <array>
#include <iostream>

#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/Process.h>

#include <fmt/format.h>

#include "config.h"
#include "driver.h"
#include "haskell_code_gen.h"
#include "yaml.h"

namespace cl = llvm::cl;

namespace {
cl::OptionCategory category{"auto-FFI Options"};
cl::opt<bool> help{"h", cl::desc{"Alias for -help"}, cl::Hidden};
cl::opt<bool> dump_config{
    "dump-config", cl::desc{"Dump configuration options to stdout and exit."},
    cl::cat{category}};
cl::opt<bool> yaml{"yaml", cl::desc{"Dump YAML for entities."},
                   cl::cat{category}};
cl::opt<bool> verbose{"verbose", cl::desc{"Print verbose output message."},
                      cl::cat{category}};
cl::list<std::string> config_files{cl::Positional, cl::desc{"[<file> ...]"},
                                   cl::cat{category}};
const char version[]{
    "auto-FFI 2020\n"
    "Copyright (C) 2020 Xie Ruifeng.\n"
    "This is free software; see the source for copying conditions.  There is "
    "ABSOLUTELY NO WARRANTY."};
}  // namespace

int main(int argc, const char* argv[]) {
  // LLVM initialization
  llvm::InitLLVM init_llvm(argc, argv);
  cl::HideUnrelatedOptions(category);

  // auto-FFI options
  cl::SetVersionPrinter([](llvm::raw_ostream& os) { os << version << '\n'; });
  cl::ParseCommandLineOptions(
      argc, argv,
      "A tool to generate Haskell FFI binding for C/C++ API.\n\n"
      "It reads the configuration from the <file>s and writes the result to "
      "the specified output files. For bug reports, please see:\n"
      "<https://github.com/Krantz-XRF/auto-FFI>.\n");

  // Help message
  if (help) {
    cl::PrintHelpMessage();
    return 0;
  }

  ffi::ffi_driver driver;

  // Configurations
  if (dump_config) {
    llvm::yaml::Output output{llvm::outs()};
    output << driver.cfg;
    return 0;
  }

  // No input, print help and exit
  if (config_files.empty()) {
    cl::PrintHelpMessage();
    return 0;
  }

  // Run on config files
  for (const auto& cfg_file : config_files) {
    auto contents = llvm::MemoryBuffer::getFile(cfg_file);
    if (auto ec = contents.getError()) {
      llvm::errs() << format(
          FMT_STRING("Failed to load configuration file \"{}\": {}\n"),
          cfg_file, ec.message());
      continue;
    }
    llvm::yaml::Input input{contents.get()->getBuffer()};
    input >> driver.cfg;
    if (input.error()) continue;

    // Diagnostics engine for configuration files
    llvm::IntrusiveRefCntPtr ids = new clang::DiagnosticIDs;
    llvm::IntrusiveRefCntPtr opts = new clang::DiagnosticOptions;
    opts->ShowColors = llvm::sys::Process::StandardOutHasColors();
    clang::TextDiagnosticPrinter printer{llvm::errs(), opts.get()};
    clang::DiagnosticsEngine diags{ids, opts, &printer};
    if (!validate_config(driver.cfg, diags)) continue;

    // Load CWD
    llvm::SmallString<128> current_path;
    llvm::sys::fs::current_path(current_path);
    if (!driver.cfg.root_directory.empty())
      llvm::sys::fs::set_current_path(driver.cfg.root_directory);

    // Compiler options
    clang::tooling::FixedCompilationDatabase compilations{
        driver.cfg.root_directory.empty() ? "." : driver.cfg.root_directory,
        driver.cfg.compiler_options};

    clang::tooling::ClangTool tool{compilations, driver.cfg.file_names};

    if (const auto status = tool.run(&driver)) return status;

    if (yaml) {
      llvm::yaml::Output output{llvm::outs()};
      output << driver.modules;
    }

    ffi::haskell_code_gen code_gen{driver.cfg};
    for (auto& [name, mod] : driver.modules) code_gen.gen_module(name, mod);

    // Recover CWD
    llvm::sys::fs::set_current_path(current_path);
  }

  return 0;
}