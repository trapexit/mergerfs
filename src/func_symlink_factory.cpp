#include "func_symlink_factory.hpp"

#include "func_symlink_all.hpp"
#include "func_symlink_epall.hpp"
#include "func_symlink_epff.hpp"
#include "func_symlink_eplfs.hpp"
#include "func_symlink_eplus.hpp"
#include "func_symlink_epmfs.hpp"
#include "func_symlink_eppfrd.hpp"
#include "func_symlink_eprand.hpp"
#include "func_symlink_erofs.hpp"
#include "func_symlink_ff.hpp"
#include "func_symlink_lfs.hpp"
#include "func_symlink_lup.hpp"
#include "func_symlink_lus.hpp"
#include "func_symlink_mfs.hpp"
#include "func_symlink_msplfs.hpp"
#include "func_symlink_msplus.hpp"
#include "func_symlink_mspmfs.hpp"
#include "func_symlink_msppfrd.hpp"
#include "func_symlink_newest.hpp"
#include "func_symlink_pfrd.hpp"
#include "func_symlink_rand.hpp"


bool
Func2::SymlinkFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::SymlinkBase>
Func2::SymlinkFactory::make(const std::string_view str_)
{
  if(str_ == "all")     return std::make_shared<Func2::SymlinkAll>();
  if(str_ == "epall")   return std::make_shared<Func2::SymlinkEPALL>();
  if(str_ == "epff")    return std::make_shared<Func2::SymlinkEPFF>();
  if(str_ == "eplfs")   return std::make_shared<Func2::SymlinkEPLFS>();
  if(str_ == "eplus")   return std::make_shared<Func2::SymlinkEPLUS>();
  if(str_ == "epmfs")   return std::make_shared<Func2::SymlinkEPMFS>();
  if(str_ == "eppfrd")  return std::make_shared<Func2::SymlinkEPPFRD>();
  if(str_ == "eprand")  return std::make_shared<Func2::SymlinkEPRAND>();
  if(str_ == "erofs")   return std::make_shared<Func2::SymlinkEROFS>();
  if(str_ == "ff")      return std::make_shared<Func2::SymlinkFF>();
  if(str_ == "lfs")     return std::make_shared<Func2::SymlinkLFS>();
  if(str_ == "lup")     return std::make_shared<Func2::SymlinkLUP>();
  if(str_ == "lus")     return std::make_shared<Func2::SymlinkLUS>();
  if(str_ == "mfs")     return std::make_shared<Func2::SymlinkMFS>();
  if(str_ == "msplfs")  return std::make_shared<Func2::SymlinkMSPLFS>();
  if(str_ == "msplus")  return std::make_shared<Func2::SymlinkMSPLUS>();
  if(str_ == "mspmfs")  return std::make_shared<Func2::SymlinkMSPMFS>();
  if(str_ == "msppfrd") return std::make_shared<Func2::SymlinkMSPPFRD>();
  if(str_ == "newest")  return std::make_shared<Func2::SymlinkNewest>();
  if(str_ == "pfrd")    return std::make_shared<Func2::SymlinkPFRD>();
  if(str_ == "rand")    return std::make_shared<Func2::SymlinkRAND>();

  return {};
}
