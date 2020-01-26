#include "config.h"

class AppContext {
 public:
  AppContext() {
  }

  virtual ~AppContext() {}

  virtual int ParseArgument(int argc, char* argv[]);

  virtual void ShowUsage(const char* app);

  Config& conf() { return conf_; }

 private:
  Config conf_;
};
