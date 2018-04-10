#include "oneflow/core/record/record_io.h"

namespace oneflow {

template<>
int32_t ReadRecord(PersistentInStream* in_stream,
                   std::vector<OFRecord>* records) {
  int64_t record_size = -1;
  for (size_t i = 0; i < records->size(); ++i) {
    if (in_stream->Read(reinterpret_cast<char*>(&record_size), sizeof(int64_t))
        == 0) {
      std::unique_ptr<char[]> buffer(new char[record_size]);
      CHECK_EQ(in_stream->Read(buffer.get(), record_size), 0);
      (*records)[i].ParseFromArray(buffer.get(), record_size);
    } else {
      return i;
    }
  }
  return records->size();
}

template<>
void WriteOneRecord(PersistentOutStream* out_stream, const OFRecord& record) {
  std::string record_bin;
  record.SerializeToString(&record_bin);
  int64_t record_size = record_bin.size();
  CHECK_GT(record_size, 0);
  (*out_stream) << record_size << record_bin;
}

}  // namespace oneflow
