#include "func_mkdir_factory.hpp"

#include "func_mkdir_all.hpp"
#include "func_mkdir_epall.hpp"
#include "func_mkdir_epff.hpp"
#include "func_mkdir_eplfs.hpp"
#include "func_mkdir_eplus.hpp"
#include "func_mkdir_epmfs.hpp"
#include "func_mkdir_eppfrd.hpp"
#include "func_mkdir_eprand.hpp"
#include "func_mkdir_erofs.hpp"
#include "func_mkdir_ff.hpp"
#include "func_mkdir_lfs.hpp"
#include "func_mkdir_lup.hpp"
#include "func_mkdir_lus.hpp"
#include "func_mkdir_mfs.hpp"
#include "func_mkdir_msplfs.hpp"
#include "func_mkdir_msplus.hpp"
#include "func_mkdir_mspmfs.hpp"
#include "func_mkdir_msppfrd.hpp"
#include "func_mkdir_newest.hpp"
#include "func_mkdir_pfrd.hpp"
#include "func_mkdir_rand.hpp"


bool
Func2::MkdirFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::MkdirBase>
Func2::MkdirFactory::make(const std::string_view str_)
{
  if(str_ == "all")     return std::make_shared<Func2::MkdirAll>();
  if(str_ == "epall")   return std::make_shared<Func2::MkdirEPALL>();
  if(str_ == "epff")    return std::make_shared<Func2::MkdirEPFF>();
  if(str_ == "eplfs")   return std::make_shared<Func2::MkdirEPLFS>();
  if(str_ == "eplus")   return std::make_shared<Func2::MkdirEPLUS>();
  if(str_ == "epmfs")   return std::make_shared<Func2::MkdirEPMFS>();
  if(str_ == "eppfrd")  return std::make_shared<Func2::MkdirEPPFRD>();
  if(str_ == "eprand")  return std::make_shared<Func2::MkdirEPRAND>();
  if(str_ == "erofs")   return std::make_shared<Func2::MkdirEROFS>();
  if(str_ == "ff")      return std::make_shared<Func2::MkdirFF>();
  if(str_ == "lfs")     return std::make_shared<Func2::MkdirLFS>();
  if(str_ == "lup")     return std::make_shared<Func2::MkdirLUP>();
  if(str_ == "lus")     return std::make_shared<Func2::MkdirLUS>();
  if(str_ == "mfs")     return std::make_shared<Func2::MkdirMFS>();
  if(str_ == "msplfs")  return std::make_shared<Func2::MkdirMSPLFS>();
  if(str_ == "msplus")  return std::make_shared<Func2::MkdirMSPLUS>();
  if(str_ == "mspmfs")  return std::make_shared<Func2::MkdirMSPMFS>();
  if(str_ == "msppfrd") return std::make_shared<Func2::MkdirMSPPFRD>();
  if(str_ == "newest")  return std::make_shared<Func2::MkdirNewest>();
  if(str_ == "pfrd")    return std::make_shared<Func2::MkdirPFRD>();
  if(str_ == "rand")    return std::make_shared<Func2::MkdirRAND>();

  return {};
}
