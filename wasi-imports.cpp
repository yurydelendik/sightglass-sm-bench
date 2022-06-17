#include <jsfriendapi.h>

#include <js/Initialization.h>
#include <js/Exception.h>
#include <js/Context.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <js/WasmModule.h>
#include <js/ArrayBuffer.h>
#include <js/ArrayBufferMaybeShared.h>

#include <fstream>
#include <mutex>

#include "wasi-imports.h"
#include "bench-state.h"

bool WasiFdClose(JSContext* cx, unsigned argc, JS::Value* vp)
{
    fprintf(stderr, "-----WasiFdClose\n");
    return false;
}
bool WasiFdSeek(JSContext* cx, unsigned argc, JS::Value* vp)
{
    fprintf(stderr, "-----WasiFdSeek\n");
    return false;

}
bool WasiFdWrite(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  JS::RootedValue memory(cx);
  if (!JS_GetProperty(cx, global, "memory", &memory)) return false;
  JS::RootedObject memoryObj(cx, &memory.toObject());
  JS::RootedValue buffer(cx);
  if (!JS_GetProperty(cx, memoryObj, "buffer", &buffer)) return false;
  uint8_t *data; bool shared; size_t length;
  JS::GetArrayBufferMaybeSharedLengthAndData(&buffer.toObject(), &length, &shared, &data);


  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();
  int len = args.get(2).toInt32();
  int written_ptr = args.get(3).toInt32();

  std::ofstream* out = nullptr;
  switch (fd) {
    case 1: out = &state->stdout; break;
    case 2: out = &state->stderr; break;
    default: return false;
  }

  uint32_t* iocv = (uint32_t*)(data + ptr);
  int total = 0;
  for (int i = 0; i < len; i++) {
      const char* p = (const char*)(data + iocv[i * 2]);
      uint32_t l = iocv[i * 2 + 1];
      out->write(p, (size_t)l);
      total += l;
  }
  *(uint32_t*)(data + written_ptr) = total;

    //   fwrite(p, 1, l, stderr);
    // fprintf(stderr, "-----WasiFdWrite %p %d %d %d %d %d\n", data, (int)length, fd, ptr, len, written_ptr);
  args.rval().setInt32(0);
  return true;
}

bool WasiProcExit(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS_ReportErrorUTF8(cx, "procexit(%d)", args.get(0).toInt32());
    return false;
}

JSObject* BuildWasiImports(JSContext *cx)
{
  JS::RootedObject wasiImportObj(cx, JS_NewPlainObject(cx));
  if (!wasiImportObj) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_close", WasiFdClose, 1, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_seek", WasiFdSeek, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_write", WasiFdWrite, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "proc_exit", WasiProcExit, 1, 0)) return nullptr;
  return wasiImportObj;
}