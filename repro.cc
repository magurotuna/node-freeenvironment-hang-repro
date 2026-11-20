// create(source): creates an Environment on the isolate, IsolateData, and
// event loop of the calling Environment, runs `source` in it, and returns an
// id. free(id): calls node::FreeEnvironment() on that Environment.
// freeLater(id, done): frees the Environment after uv_run() returns, and then
// calls done().
#include <node.h>
#include <uv.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::vector<node::Environment*> envs;

void Create(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  node::Environment* parent =
      node::GetCurrentEnvironment(isolate->GetCurrentContext());
  v8::Local<v8::Context> context = node::NewContext(isolate);
  node::Environment* env =
      node::CreateEnvironment(node::GetEnvironmentIsolateData(parent),
                              context,
                              {"node"},
                              {},
                              node::EnvironmentFlags::kNoCreateInspector,
                              node::AllocateEnvironmentThreadId());
  v8::String::Utf8Value source(isolate, args[0]);
  {
    v8::Context::Scope context_scope(context);
    (void)node::LoadEnvironment(env,
                                std::string_view(*source, source.length()));
  }
  envs.push_back(env);
  args.GetReturnValue().Set(static_cast<uint32_t>(envs.size() - 1));
}

void Free(const v8::FunctionCallbackInfo<v8::Value>& args) {
  uint32_t id = args[0].As<v8::Uint32>()->Value();
  node::FreeEnvironment(envs[id]);
  envs[id] = nullptr;
}

struct PendingFree {
  node::Environment* env;
  v8::Global<v8::Function> done;
};
std::vector<PendingFree> pending;
uv_idle_t idle;
uv_check_t check;
node::MultiIsolatePlatform* platform = nullptr;

class FreeTask : public v8::Task {
 public:
  // SpinEventLoop() runs this task from DrainTasks(), after uv_run() returns.
  void Run() override {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    for (PendingFree& p : std::exchange(pending, {})) {
      node::FreeEnvironment(p.env);
      (void)node::MakeCallback(isolate, context->Global(),
                               p.done.Get(isolate), 0, nullptr, {0, 0});
    }
  }
};

// Runs in the check phase, at the end of this loop iteration.
void OnCheck(uv_check_t* handle) {
  uv_idle_stop(&idle);
  uv_check_stop(handle);
  // uv_run() returns after this iteration, without polling again.
  uv_stop(handle->loop);
  // Pass the location: Node.js 24 on Windows does not export the default,
  // v8::SourceLocation::Current().
  platform->GetForegroundTaskRunner(v8::Isolate::GetCurrent())
      ->PostTask(std::make_unique<FreeTask>(), v8::SourceLocation());
}

void FreeLater(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  if (platform == nullptr) {
    platform = node::GetMultiIsolatePlatform(
        node::GetCurrentEnvironment(isolate->GetCurrentContext()));
    uv_loop_t* loop = node::GetCurrentEventLoop(isolate);
    uv_idle_init(loop, &idle);
    uv_check_init(loop, &check);
  }
  uint32_t id = args[0].As<v8::Uint32>()->Value();
  v8::Local<v8::Function> done = args[1].As<v8::Function>();
  pending.push_back({envs[id], v8::Global<v8::Function>(isolate, done)});
  envs[id] = nullptr;
  // An active idle handle keeps the poll phase from blocking, so the check
  // phase comes soon.
  uv_idle_start(&idle, [](uv_idle_t*) {});
  uv_check_start(&check, OnCheck);
}

}  // namespace

NODE_MODULE_INIT(/* exports, module, context */) {
  NODE_SET_METHOD(exports, "create", Create);
  NODE_SET_METHOD(exports, "free", Free);
  NODE_SET_METHOD(exports, "freeLater", FreeLater);
}
