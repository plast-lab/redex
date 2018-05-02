/* Reads a list of method names from a file and removes them from
   their containing classes. The input file is assumed to contain one
   JVM-style method descriptor per line (as in names contained in
   .smali files). Compared to opt/simpleinline/Deleter, Remover does
   not run checks (like is_concrete() or can_delete()). */

#include <fstream>
#include "Remover.h"
#include "DexUtil.h"

typedef std::set<std::pair<DexClass*, DexMethod*> > CMethods;
typedef std::set<std::pair<std::string, std::string>> CMethodStrs;

// The name of the methods file is currently hardcoded.
std::string fName = "methods_to_remove.txt";

/* Given a DexMethod, generates its JVM descriptor string. */
std::string gen_JVM_descriptor(DexMethod *m) {
  // Build result by concatenating method name and signature.
  std::stringstream ss;
  ss << m->get_name()->c_str();
  ss << show(m->get_proto());
  return ss.str();
}

/* Given a list of class/method pairs, finds all candidates for removal,
   then proceeds to delete them. */
void remove_methods(std::vector<DexClass*>& classes, CMethodStrs cMethodStrs) {
  CMethods methods;
  std::cout << "Collecting methods..." << std::endl;
  int64_t markedForRemoval = 0;
  for (auto const& cls : classes) {
    std::string clsQName = cls->get_name()->c_str();
    for (auto const& dm : cls->get_dmethods()) {
      auto const m = std::make_pair(clsQName, gen_JVM_descriptor(dm));
      if (cMethodStrs.find(m) != cMethodStrs.end()) {
        // std::cout << "Found dmethod " << m.first << ":" << m.second<< std::endl;
        methods.insert(std::make_pair(cls, dm));
        markedForRemoval++;
      }
    }
    for (auto const& vm : cls->get_vmethods()) {
      auto const m = std::make_pair(clsQName, gen_JVM_descriptor(vm));
      if (cMethodStrs.find(m) != cMethodStrs.end()) {
        // std::cout << "Found vmethod " << m.first << ":" << m.second<< std::endl;
        methods.insert(std::make_pair(cls, vm));
        markedForRemoval++;
      }
    }
  }

  // Finally remove marked methods.
  for (auto const& cm : methods) {
    cm.first->remove_method(cm.second);
    std::cout << "Removing " << cm.first->get_name()->c_str() << "::"
                             << cm.second->get_name()->c_str() << std::endl;
  }
  std::cout << "Removed " << methods.size() << " methods (from " << markedForRemoval << " marked)." << std::endl;
}

void RemoverPass::run_pass(DexStoresVector& dexen, ConfigFiles& cfg, PassManager& mgr) {
  CMethodStrs cMethodStrs;
  std::ifstream cFile;
  std::string cName, mName;

  cFile.open(fName);
  if (!cFile.good()) {
    return;
  }
  std::cout << "Reading list of methods to remove from " << fName << "..." << std::endl;
  while (cFile.good()) {
    std::getline(cFile, cName, ':');
    std::getline(cFile, mName, '\n');
    if (!cName.empty() && !mName.empty()) {
      cMethodStrs.insert(std::make_pair(cName, mName));
    }
  }
  cFile.close();
  std::cout << "Read " << cMethodStrs.size() << " methods." << std::endl;

  Scope scope = build_class_scope(dexen);
  std::cout << "Classes removal pass" << std::endl;
  remove_methods(scope, cMethodStrs);
}

static RemoverPass s_pass;