#include "func_mknod_factory.hpp"

#include "func_mknod_all.hpp"
#include "func_mknod_epall.hpp"
#include "func_mknod_epff.hpp"
#include "func_mknod_eplfs.hpp"
#include "func_mknod_eplus.hpp"
#include "func_mknod_epmfs.hpp"
#include "func_mknod_eppfrd.hpp"
#include "func_mknod_eprand.hpp"
#include "func_mknod_erofs.hpp"
#include "func_mknod_ff.hpp"
#include "func_mknod_lfs.hpp"
#include "func_mknod_lup.hpp"
#include "func_mknod_lus.hpp"
#include "func_mknod_mfs.hpp"
#include "func_mknod_msplfs.hpp"
#include "func_mknod_msplus.hpp"
#include "func_mknod_mspmfs.hpp"
#include "func_mknod_msppfrd.hpp"
#include "func_mknod_newest.hpp"
#include "func_mknod_pfrd.hpp"
#include "func_mknod_rand.hpp"


bool
Func2::MknodFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::MknodBase>
Func2::MknodFactory::make(const std::string_view str_)
{
  if(str_ == "all")     return std::make_shared<Func2::MknodAll>();
  if(str_ == "epall")   return std::make_shared<Func2::MknodEPALL>();
  if(str_ == "epff")    return std::make_shared<Func2::MknodEPFF>();
  if(str_ == "eplfs")   return std::make_shared<Func2::MknodEPLFS>();
  if(str_ == "eplus")   return std::make_shared<Func2::MknodEPLUS>();
  if(str_ == "epmfs")   return std::make_shared<Func2::MknodEPMFS>();
  if(str_ == "eppfrd")  return std::make_shared<Func2::MknodEPPFRD>();
  if(str_ == "eprand")  return std::make_shared<Func2::MknodEPRAND>();
  if(str_ == "erofs")   return std::make_shared<Func2::MknodEROFS>();
  if(str_ == "ff")      return std::make_shared<Func2::MknodFF>();
  if(str_ == "lfs")     return std::make_shared<Func2::MknodLFS>();
  if(str_ == "lup")     return std::make_shared<Func2::MknodLUP>();
  if(str_ == "lus")     return std::make_shared<Func2::MknodLUS>();
  if(str_ == "mfs")     return std::make_shared<Func2::MknodMFS>();
  if(str_ == "msplfs")  return std::make_shared<Func2::MknodMSPLFS>();
  if(str_ == "msplus")  return std::make_shared<Func2::MknodMSPLUS>();
  if(str_ == "mspmfs")  return std::make_shared<Func2::MknodMSPMFS>();
  if(str_ == "msppfrd") return std::make_shared<Func2::MknodMSPPFRD>();
  if(str_ == "newest")  return std::make_shared<Func2::MknodNewest>();
  if(str_ == "pfrd")    return std::make_shared<Func2::MknodPFRD>();
  if(str_ == "rand")    return std::make_shared<Func2::MknodRAND>();

  return {};
}
