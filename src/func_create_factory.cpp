#include "func_create_factory.hpp"

#include "func_create_all.hpp"
#include "func_create_epall.hpp"
#include "func_create_epff.hpp"
#include "func_create_eplfs.hpp"
#include "func_create_eplus.hpp"
#include "func_create_epmfs.hpp"
#include "func_create_eppfrd.hpp"
#include "func_create_eprand.hpp"
#include "func_create_erofs.hpp"
#include "func_create_ff.hpp"
#include "func_create_lfs.hpp"
#include "func_create_lup.hpp"
#include "func_create_lus.hpp"
#include "func_create_mfs.hpp"
#include "func_create_msplfs.hpp"
#include "func_create_msplus.hpp"
#include "func_create_mspmfs.hpp"
#include "func_create_msppfrd.hpp"
#include "func_create_newest.hpp"
#include "func_create_pfrd.hpp"
#include "func_create_rand.hpp"


bool
Func2::CreateFactory::valid(const std::string str_)
{
  return (bool)make(str_);
}

std::shared_ptr<Func2::CreateBase>
Func2::CreateFactory::make(const std::string_view str_)
{
  if(str_ == "all")     return std::make_shared<Func2::CreateAll>();
  if(str_ == "epall")   return std::make_shared<Func2::CreateEPALL>();
  if(str_ == "epff")    return std::make_shared<Func2::CreateEPFF>();
  if(str_ == "eplfs")   return std::make_shared<Func2::CreateEPLFS>();
  if(str_ == "eplus")   return std::make_shared<Func2::CreateEPLUS>();
  if(str_ == "epmfs")   return std::make_shared<Func2::CreateEPMFS>();
  if(str_ == "eppfrd")  return std::make_shared<Func2::CreateEPPFRD>();
  if(str_ == "eprand")  return std::make_shared<Func2::CreateEPRAND>();
  if(str_ == "erofs")   return std::make_shared<Func2::CreateEROFS>();
  if(str_ == "ff")      return std::make_shared<Func2::CreateFF>();
  if(str_ == "lfs")     return std::make_shared<Func2::CreateLFS>();
  if(str_ == "lup")     return std::make_shared<Func2::CreateLUP>();
  if(str_ == "lus")     return std::make_shared<Func2::CreateLUS>();
  if(str_ == "mfs")     return std::make_shared<Func2::CreateMFS>();
  if(str_ == "msplfs")  return std::make_shared<Func2::CreateMSPLFS>();
  if(str_ == "msplus")  return std::make_shared<Func2::CreateMSPLUS>();
  if(str_ == "mspmfs")  return std::make_shared<Func2::CreateMSPMFS>();
  if(str_ == "msppfrd") return std::make_shared<Func2::CreateMSPPFRD>();
  if(str_ == "newest")  return std::make_shared<Func2::CreateNewest>();
  if(str_ == "pfrd")    return std::make_shared<Func2::CreatePFRD>();
  if(str_ == "rand")    return std::make_shared<Func2::CreateRAND>();

  return {};
}
