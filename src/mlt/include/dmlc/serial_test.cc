#include "dmlc/io.h"
#include "dmlc/memory_io.h"

#include <string>
#include <vector>

struct SimpleStructure {
  int payload_length;
  char payload[0];
};

using namespace dmlc;

void task1() {
  printf("task1\n");
  char* buffer = static_cast<char*>(malloc(sizeof(SimpleStructure) + 10));
  MemoryFixedSizeStream strm(buffer, sizeof(SimpleStructure) + 10);

  SimpleStructure* pod = reinterpret_cast<SimpleStructure*>(buffer);
  pod->payload_length = 10;
  strncpy(pod->payload, "haha", 5);
  //strm.Read(pod, sizeof(SimpleStructure) + 10);
  printf("%s\n", pod->payload);

  
  char* buffer2 = static_cast<char*>(malloc(sizeof(SimpleStructure) + 10));
  SimpleStructure* pod2 = reinterpret_cast<SimpleStructure*>(buffer2);
  strm.Read(pod2, sizeof(SimpleStructure) + 10);

  printf("%d\n", pod2->payload_length);
  printf("%s\n", pod2->payload);

  free(buffer);
  free(buffer2);
}

void task2() {
  printf("task2\n");
  std::vector<std::string> hosts{"1.1.1.1:22", "1.1.1.2:33", "2.2.2.2:344", "3.3.3.3:1234"};
  
  std::string str;
  //MemoryStringStream strm(&str);
  std::unique_ptr<SeekStream> strm = std::make_unique<MemoryStringStream>(&str);
  strm->Write(4);
  strm->Write<std::vector<std::string>>(hosts);


  int num = 0;
  std::vector<std::string> res;
  std::unique_ptr<SeekStream> r_strm = std::make_unique<MemoryStringStream>(&str);
  r_strm->Read(&num);
  r_strm->Read(&res);
  printf("%d\n", num);
  printf("%ld\n", res.size());
  for (size_t i = 0; i < res.size(); i++) {
    printf("%s%c", res[i].c_str(), i + 1 == res.size() ? '\n' : ',');
  }
}

int main() {
  task1();
  task2();
  return 0;
}
