#pragma once
class AsyncResponse{
    public:
    uint8_t* buffer;
    size_t buffer_len;

    AsyncResponse(uint32_t ns, flatbuffers::FlatBufferBuilder* b){
        uint8_t* bp=b->GetBufferPointer();
        size_t flatbuffer_data_size = b->GetSize();
        buffer_len=sizeof(uint32_t) + flatbuffer_data_size;
        buffer = new uint8_t[buffer_len];
        ESP_LOGD(TAG, "Preparing a buffer of size %d+%d", sizeof(uint32_t), flatbuffer_data_size);
        ((uint32_t*)buffer)[0]=ns;
        std::memcpy(buffer+sizeof(uint32_t), bp, flatbuffer_data_size);
    }

    ~AsyncResponse(){
        delete[] buffer;
    }
};
