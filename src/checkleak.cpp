#include <android/log.h>
#include <backtrace/Backtrace.h>
#include <pthread.h>
#include <sys/system_properties.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#define TAG       "checkmem"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// if android opt dlopen lib, need call this func
// always do not need call this func
void triger_openlib() {
    LOGE("just triger linker open lib");
}

void __attribute__((constructor)) checklib_thread(void);

void checkLeak();
void* loop_func(void* data) {
    char value[PROP_VALUE_MAX] = {0};

    for (;;) {
        sleep(5);
        __system_property_get("triger_libcheck", value);
        if (!strcmp(value, "1")) {
            LOGE("+++++++ into check");
            checkLeak();
            __system_property_set("triger_libcheck", "0");
            LOGE("+++++++ end check");
        } else {
            LOGE("checkleak wait triger!");
        }
    }

    return data;
}

void checklib_thread() {
    pthread_t t;
    LOGE("%s: start!", __func__);
    pthread_create(&t, NULL, loop_func, NULL);
}

using namespace std;
extern "C" void debug_get_malloc_leak_info(
        uint8_t** info, size_t* overallSize, size_t* infoSize, size_t* totalMemory,
        size_t* backtraceSize);

extern "C" void debug_free_malloc_leak_info(uint8_t* info);
extern "C" void debug_dump_heap(const char* file_name);

void checkLeak() {
    ostringstream fileName;
    ifstream in("/data/local/tmp/checkleak");
    if (!in) {
        system("mkdir /data/local/tmp/checkleak");
    }
    fileName << "/data/local/tmp/checkleak/checkleak_" << getpid() << "_"
             << time((time_t*)NULL) << ".csv";

    if (1) {
        ofstream outFile(fileName.str());
        std::unique_ptr<Backtrace> backtrace(
                Backtrace::Create(BACKTRACE_CURRENT_PROCESS, BACKTRACE_CURRENT_THREAD));

        backtrace->Unwind(0);

        if (outFile)
            outFile << "size,duplications,backtrace" << endl;
        else
            return;

        uint8_t* info = NULL;
        size_t overallSize = 0, infoSize = 0, totalMemory = 0, backtraceSize = 0;
        debug_get_malloc_leak_info(
                &info, &overallSize, &infoSize, &totalMemory, &backtraceSize);

        outFile << "summay info:"
                << " overallSize: " << overallSize << " infoSize:" << infoSize
                << " totalMemory:" << totalMemory << " backtraceSize:" << backtraceSize
                << endl;
        if (!info)
            return;

        size_t recordCount = overallSize / infoSize;
        size_t index = 0;
        uint8_t* ptr = info;

        while (index < recordCount) {
            size_t allocMemSize = *reinterpret_cast<size_t*>(ptr);
            ptr += sizeof(size_t);
            size_t allocTimes = *reinterpret_cast<size_t*>(ptr);
            ptr += sizeof(size_t);

            outFile << allocMemSize << "," << allocTimes << ",\"";

            for (size_t i = 0; i < backtraceSize; ++i) {
                uintptr_t pc =
                        *reinterpret_cast<uintptr_t*>(ptr + i * sizeof(uintptr_t));
                if (!pc)
                    break;

                backtrace_map_t map;
                backtrace->FillInMap(pc, &map);
                // unsigned long long  offset = 0;
                uint64_t offset = 0;
                string funcName = backtrace->GetFunctionName(pc, &offset);

                // uintptr_t relative_pc = BacktraceMap::GetRelativePc(map, pc);
                // unsigned long long relative_pc = offset;
                uint64_t relative_pc = pc - map.start + map.load_bias;
                outFile << "#" << setw(2) << setfill('0') << i << " pc 0x" << hex
                        << relative_pc << dec << " " << map.name;
                if (!funcName.empty()) {
                    outFile << " (" << funcName << "+0x" << hex << offset << dec << ")";
                }
                outFile << endl;
            }

            outFile << "\"" << endl;
            ptr += sizeof(uintptr_t) * backtraceSize;
            ++index;
        }
        debug_free_malloc_leak_info(info);
        outFile << "+++++++++++++++ end \"" << endl;
        outFile.close();
    } else {
        debug_dump_heap(fileName.str().c_str());
    }
}
