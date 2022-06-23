#include <jsfriendapi.h>

#include <js/Initialization.h>
#include <js/Exception.h>
#include <js/Context.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <js/WasmModule.h>
#include <js/ArrayBuffer.h>
#include <js/ArrayBufferMaybeShared.h>
#include <js/BigInt.h>

#include <fstream>
#include <random>
#include <sys/stat.h>
#include <time.h>

#include "wasi-imports.h"
#include "bench-state.h"
#include "wasi-api.h"

static bool GetWasmMemory(JSContext* cx, JS::HandleObject global, uint8_t **data_out, size_t *len_out)
{
  JS::RootedValue memory(cx);
  if (!JS_GetProperty(cx, global, "memory", &memory)) return false;
  JS::RootedObject memoryObj(cx, &memory.toObject());
  JS::RootedValue buffer(cx);
  if (!JS_GetProperty(cx, memoryObj, "buffer", &buffer)) return false;
  uint8_t *data; bool shared; size_t length;
  JS::GetArrayBufferMaybeSharedLengthAndData(&buffer.toObject(), &length, &shared, &data);
  *data_out = data;
  *len_out = length;
  return true;
}

bool WasiFdClose(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();
  if (fd >= state->fd_table.size() || !state->fd_table[fd]) {
    fprintf(stderr, "-----WasiFdClose %d\n", fd);
    return false;
  }

  state->fd_table[fd].reset();
  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
static void GetFilestat(const std::string &path, wasi_api::__wasi_filestat_t *p)
{
  struct stat buf = {0};
  stat(path.c_str(), &buf);
  p->dev = buf.st_dev;
  p->ino = buf.st_ino;
  p->filetype = (buf.st_mode & S_IFDIR) ? __WASI_FILETYPE_DIRECTORY : __WASI_FILETYPE_REGULAR_FILE;
  p->nlink = buf.st_nlink;
  p->size = buf.st_size;
  p->atim = buf.st_atime;
  p->mtim = buf.st_mtime;
  p->ctim = buf.st_ctime;  
}
bool WasiFdFilestatGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();

  if (fd >= state->fd_table.size() || !state->fd_table[fd]) {
    fprintf(stderr, "-----WasiFdFilestatGet %d\n", fd);
    return false;
  }

  wasi_api::__wasi_filestat_t *p = (wasi_api::__wasi_filestat_t*)(data + ptr);
  if (state->fd_table[fd]->path.empty()) {
    p->dev = -1;
    p->ino = fd;
    p->filetype = __WASI_FILETYPE_CHARACTER_DEVICE;
  } else {
    GetFilestat(state->fd_table[fd]->path, p);
    // TODO assert p->filetype == state->fd_table[fd]->is_dir
  }
  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiFdFdstatGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();

  if (fd >= state->fd_table.size() || !state->fd_table[fd]) {
    fprintf(stderr, "-----WasiFdFdstatGet %d\n", fd);
    return false;
  }

  wasi_api::__wasi_fdstat_t *p = (wasi_api::__wasi_fdstat_t*)(data + ptr);
  struct stat buf;
  stat(state->fd_table[fd]->path.c_str(), &buf);
  p->fs_flags = 0;
  p->fs_rights_base = ~0;
  p->fs_rights_inheriting = ~0;
  p->fs_filetype =
    state->fd_table[fd]->path.empty() ? __WASI_FILETYPE_CHARACTER_DEVICE :
    state->fd_table[fd]->is_dir ? __WASI_FILETYPE_DIRECTORY : __WASI_FILETYPE_REGULAR_FILE;
  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiFdFdstatSetFlags(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int flags = args.get(1).toInt32();
  fprintf(stderr, "-----WasiFdFilestatSetFlags %d %d\n", fd, flags);
  return false;
}

bool WasiFdSeek(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int64_t offset = JS::ToBigInt64(args.get(1).toBigInt());
  int whence = args.get(2).toInt32();
  int ptr = args.get(3).toInt32();
  if (fd >= state->fd_table.size() || !state->fd_table[fd] ||
     state->fd_table[fd]->is_dir) {
    fprintf(stderr, "-----WasiFdSeek %d\n", fd);
    return false;
  }
  std::fstream* s = &*(state->fd_table[fd]->file);
  s->seekg(offset, (std::ios_base::seekdir)whence);
  *(int64_t *)(data + ptr) = s->tellg();
  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiFdRead(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();
  int len = args.get(2).toInt32();
  int read_ptr = args.get(3).toInt32();

  if (fd == -1){
    args.rval().setInt32(__WASI_ERRNO_BADF);
    return true;
  }

  if (fd >= state->fd_table.size() || !state->fd_table[fd]) {
    fprintf(stderr, "-----WasiFdRead %d %d\n", fd, len); 
    return false;
  }
  std::istream* in = &*(state->fd_table[fd]->file);
  
  wasi_api::__wasi_iovec_t *iocs = (wasi_api::__wasi_iovec_t *)(data + ptr);
  int total = 0;
  for (int i = 0; i < len; i++) {
      char* p = (char*)(data + iocs[i].buf);
      uint32_t l = iocs[i].buf_len;
      in->read(p, (size_t)l);
      if (!*in) {
        total += in->gcount();
        break;
      }
      total += l;
  }
  *(uint32_t*)(data + read_ptr) = total;

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiFdWrite(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();
  int len = args.get(2).toInt32();
  int written_ptr = args.get(3).toInt32();

  if (fd >= state->fd_table.size() || !state->fd_table[fd]) {
    fprintf(stderr, "-----WasiFdWrite\n"); 
    return false;
  }
  std::ostream* out = &*(state->fd_table[fd]->file);

  wasi_api::__wasi_iovec_t *iocs = (wasi_api::__wasi_iovec_t *)(data + ptr);
  int total = 0;
  for (int i = 0; i < len; i++) {
      const char* p = (const char*)(data + iocs[i].buf);
      uint32_t l = iocs[i].buf_len;
      out->write(p, (size_t)l);
      total += l;
  }
  *(uint32_t*)(data + written_ptr) = total;

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiPathOpen(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int dir_fd = args.get(0).toInt32();
  int dir_flags = args.get(1).toInt32();
  int path_ptr = args.get(2).toInt32();
  int path_len = args.get(3).toInt32();
  int o_flags = args.get(4).toInt32();
  int fd_out = args.get(8).toInt32();

  if (dir_fd == PREOPEN_DIR_FD) {
    std::string path((const char*)(data + path_ptr), path_len);
    std::string full_path = state->fd_table[dir_fd]->path + "/" + path;
    std::fstream f(full_path, std::ios_base::in);
    FdEntry entry(std::move(f), std::move(full_path));
    state->fd_table.push_back(std::move(entry));
  
    *(uint32_t*)(data + fd_out) = state->fd_table.size() - 1;
    args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  } else {
    args.rval().setInt32(__WASI_ERRNO_BADF);
  }
  return true;
}
bool WasiPathFilestatGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int path_ptr = args.get(2).toInt32();
  int path_len = args.get(3).toInt32();
  int ptr = args.get(4).toInt32();

  if (fd >= state->fd_table.size() || !state->fd_table[fd] ||
      !state->fd_table[fd]->is_dir) {
    
    return false;
  }

  std::string path((const char*)(data + path_ptr), path_len);
  std::string full_path = state->fd_table[fd]->path + "/" + path;

  wasi_api::__wasi_filestat_t *p = (wasi_api::__wasi_filestat_t*)(data + ptr);
  GetFilestat(full_path, p);
  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiPathRemoveDirectory(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiPathRemoveDirectory\n");
  return false;
}
bool WasiPathUnlinkFile(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiPathUnlinkFile\n");
  return false;
}

bool WasiFdPrestatGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();
  if (fd == PREOPEN_DIR_FD) {
    wasi_api::__wasi_prestat_t *entry = (wasi_api::__wasi_prestat_t*)(data + ptr);
    entry->tag = 0;
    entry->u.dir.pr_name_len = 1;
    args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  } else {
    args.rval().setInt32(__WASI_ERRNO_BADF);
  }
  return true;
}
bool WasiFdPrestatDirName(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int fd = args.get(0).toInt32();
  int ptr = args.get(1).toInt32();
  int len = args.get(2).toInt32();
  if (fd == PREOPEN_DIR_FD) {
    if (len < 1) {
      args.rval().setInt32( __WASI_ERRNO_NAMETOOLONG);
    } else {
      *(uint8_t*)(data + ptr) = '.';
      args.rval().setInt32(__WASI_ERRNO_SUCCESS);
    }
  } else {
    args.rval().setInt32(__WASI_ERRNO_BADF);
  }
  return true;
}
bool WasiProcExit(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS_ReportErrorUTF8(cx, "procexit(%d)", args.get(0).toInt32());
    return false;
}
bool WasiEnvironSizesGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int count_ptr = args.get(0).toInt32();
  int size_ptr = args.get(1).toInt32();
  *(uint32_t*)(data + count_ptr) = 0;
  *(uint32_t*)(data + size_ptr) = 0;

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiEnvironGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  fprintf(stderr, "-----WasiEnvironGet\n");
  return false;
}
bool WasiArgsSizesGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int count_ptr = args.get(0).toInt32();
  int size_ptr = args.get(1).toInt32();
  *(uint32_t*)(data + count_ptr) = 1;
  *(uint32_t*)(data + size_ptr) = 2;

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiArgsGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int argv_ptr = args.get(0).toInt32();
  int buf_ptr = args.get(1).toInt32();
  *(uint32_t*)(data + argv_ptr) = buf_ptr;
  *(uint8_t*)(data + buf_ptr) = 'a';
  *(uint8_t*)(data + buf_ptr + 1) = 0;

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}
bool WasiClockResGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int id = args.get(0).toInt32();
  int ret_ptr = args.get(1).toInt32();
  if (id >= 2) {
    fprintf(stderr, "-----WasiClockResGet\n");
    return false;
  }

  struct timespec ts;
  clock_getres((clockid_t)id, &ts);
  *(int64_t*)(data + ret_ptr) = ts.tv_sec * 1000000000ll + ts.tv_nsec;

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}

bool WasiClockTimeGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  BenchState* state = JS::GetMaybePtrFromReservedSlot<BenchState>(global, 0);
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int id = args.get(0).toInt32();
  int ret_ptr = args.get(2).toInt32();
  if (id >= 2) {
    fprintf(stderr, "-----WasiClockTimeGet\n");
    return false;
  }

  struct timespec ts;
  clock_gettime((clockid_t)id, &ts);
  *(int64_t*)(data + ret_ptr) = ts.tv_sec * 1000000000ll + ts.tv_nsec;

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}

bool WasiRandomGet(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  uint8_t *data; size_t length;
  if (!GetWasmMemory(cx, global, &data, &length)) return false;

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  int buf_ptr = args.get(0).toInt32();
  int buf_len = args.get(1).toInt32();

  uint8_t *buf = (uint8_t*)(data + buf_ptr);

  using random_bytes_engine = std::independent_bits_engine<
    std::default_random_engine, CHAR_BIT, unsigned char>;
  
  random_bytes_engine rbe;
  for (int i = 0; i < buf_len; i++) {
    buf[i] = rbe();
  }

  args.rval().setInt32(__WASI_ERRNO_SUCCESS);
  return true;
}

JSObject* BuildWasiImports(JSContext *cx)
{
  JS::RootedObject wasiImportObj(cx, JS_NewPlainObject(cx));
  if (!wasiImportObj) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_close", WasiFdClose, 1, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_filestat_get", WasiFdFilestatGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_fdstat_get", WasiFdFdstatGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_fdstat_set_flags", WasiFdFdstatSetFlags, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_seek", WasiFdSeek, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_read", WasiFdRead, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_write", WasiFdWrite, 4, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "path_open", WasiPathOpen, 9, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "path_remove_directory", WasiPathRemoveDirectory, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "path_unlink_file", WasiPathUnlinkFile, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "path_filestat_get", WasiPathFilestatGet, 5, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_prestat_get", WasiFdPrestatGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "fd_prestat_dir_name", WasiFdPrestatDirName, 3, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "proc_exit", WasiProcExit, 1, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "environ_sizes_get", WasiEnvironSizesGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "environ_get", WasiEnvironGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "args_sizes_get", WasiArgsSizesGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "args_get", WasiArgsGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "clock_res_get", WasiClockResGet, 2, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "clock_time_get", WasiClockTimeGet, 3, 0)) return nullptr;
  if (!JS_DefineFunction(cx, wasiImportObj, "random_get", WasiRandomGet, 2, 0)) return nullptr;
  return wasiImportObj;
}
