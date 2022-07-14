// Copyright 2013 Google Inc. All Rights Reserved.
// Author: dnovillo@google.com (Diego Novillo)

// This program creates an LLVM profile from an AutoFDO source.

#include "third_party/abseil/absl/flags/flag.h"
#include "third_party/abseil/absl/strings/match.h"
#if defined(HAVE_LLVM)
#include <fstream>
#include <memory>
#include <string>

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "llvm_profile_writer.h"
#include "llvm_propeller_code_layout.h"
#include "llvm_propeller_options.pb.h"
#include "llvm_propeller_options_builder.h"
#include "llvm_propeller_profile_writer.h"
#include "perfdata_reader.h"
#include "profile_creator.h"
#include "third_party/abseil/absl/status/status.h"
#include "third_party/abseil/absl/strings/str_split.h"
#include "third_party/abseil/absl/flags/parse.h"
#include "third_party/abseil/absl/flags/usage.h"
ABSL_FLAG(std::string, binary, "a.out", "Binary file name");
ABSL_FLAG(std::string, profile, "perf.data",
          "Input profile file name. When --format=propeller, this accepts "
          "multiple profile file names concatnated by ';' and if the file name "
          "has prefix \"@\", then the profile is treated as a list file whose "
          "lines are interpreted as input profile paths.");
ABSL_FLAG(std::string, out, "", "Output profile file name");
ABSL_FLAG(std::string, propeller_symorder, "",
          "Propeller symbol ordering output file name.");

using namespace devtools_crosstool_autofdo;

PropellerOptions CreatePropellerOptionsFromFlags() {
  PropellerOptionsBuilder option_builder;
  std::string pstr = absl::GetFlag(FLAGS_profile);
  if (!pstr.empty() && pstr[0] == '@') {
    std::ifstream fin(pstr.substr(1));
    std::string pf;
    while (std::getline(fin, pf)) {
      if (!pf.empty() && pf[0] != '#') {
        option_builder.AddPerfNames(pf);
      }
    }
  } else {
    std::vector<std::string> perf_files = absl::StrSplit(pstr, ';');
    for (const std::string &pf : perf_files)
      if (!pf.empty()) option_builder.AddPerfNames(pf);
  }
  return PropellerOptions(
      option_builder.SetBinaryName(absl::GetFlag(FLAGS_binary))
          .SetClusterOutName(absl::GetFlag(FLAGS_out))
          .SetSymbolOrderOutName("./symbol_order.txt")
          .SetProfiledBinaryName("")
          .SetIgnoreBuildId(false));
}

static std::set<CFGNode*> visited_nodes;

static void DFS(CFGNode* node, std::function<void(CFGNode* node)> on_node, std::function<void(CFGEdge*)> on_edge = nullptr) {
  if (visited_nodes.count(node)) return;
  visited_nodes.insert(node);
  if (on_node) on_node(node);
  
  for (auto& edges : {node->intra_outs()}) {
    for (auto edge : edges) {
      if (on_edge) on_edge(edge);
      DFS(edge->sink(), on_node, on_edge);
    }
  }
}

int main(int argc, char **argv) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);

  std::unique_ptr<PropellerProfWriter> writer =
      PropellerProfWriter::Create(CreatePropellerOptionsFromFlags());

  auto cfgs = writer->whole_program_info()->GetHotCfgs();
  // for (auto* cfg : cfgs) {
  //   std::cout << "CFG:" << cfg->GetPrimaryName().str() << std::endl;
  //   // cfg->WriteAsDotGraph(cfg->GetPrimaryName());
  //   visited_nodes.clear();
  //   DFS(cfg->GetEntryNode(), nullptr, [] (CFGEdge* edge) {
  //     std::cout << edge->src()->addr() << " (" << edge->weight()<< ")-> " << edge->sink()->addr() << std::endl;
  //   });
  // }

  // std::cout << "--------------------------------------------" << std::endl;

  for (auto* cfg : cfgs) {
    std::cout << "CFG:" << cfg->GetPrimaryName().str() << std::endl;
    visited_nodes.clear();
    DFS(cfg->GetEntryNode(), [] (CFGNode* node) {
      std::cout << node->bb_index() << " ";
    });
    std::cout << std::endl;
  }


  return 0;
}

#else
#include <stdio.h>
#include "third_party/abseil/absl/flags/parse.h"
#include "third_party/abseil/absl/flags/usage.h"
int main(int argc, char **argv) {
  fprintf(stderr,
          "ERROR: LLVM support was not enabled in this configuration.\nPlease "
          "configure and rebuild with:\n\n$ ./configure "
          "--with-llvm=<path-to-llvm-config>\n\n");
  return -1;
}
#endif  // HAVE_LLVM