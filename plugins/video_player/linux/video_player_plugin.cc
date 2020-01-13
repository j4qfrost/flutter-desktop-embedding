// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "plugins/video_player/linux/video_player_plugin.h"

#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <string>
#include <map>

using std::string;
using std::cout;
using std::endl;

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>
#include <flutter/plugin_registrar_glfw.h>


namespace plugins_video_player {

namespace {
// See video_player.dart for documentation.
const char kChannelName[] = "flutter.io/videoPlayer";
const char kInitMethod[] = "init";
const char kCreateMethod[] = "create";
const char kPlayMethod[] = "play";
const char kPauseMethod[] = "pause";
const char kPositionMethod[] = "position";
const char kDisposeMethod[] = "dispose";
}

using flutter::EncodableMap;
using flutter::EncodableValue;

typedef FlutterResponder std::unique_ptr<flutter::MethodResult<EncodableValue>> result;

class VideoPlayerPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar);

  virtual ~VideoPlayerPlugin();

 protected:
  void Create(const EncodableValue& arguments, FlutterResponder result);
  void Play(const EncodableValue& arguments, FlutterResponder result);
  void Pause(const EncodableValue& arguments, FlutterResponder result);
  void Position(const EncodableValue& arguments, FlutterResponder result);
  void Dispose(const EncodableValue& arguments, FlutterResponder result);

 private:
  // Creates a plugin that communicates on the given channel.
  VideoPlayerPlugin(
      std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel);

  // Called when a method is called on |channel_|;
  void HandleMethodCall(
      const flutter::MethodCall<EncodableValue> &method_call, FlutterResponder result);

  // The MethodChannel used for communication with the Flutter engine.
  std::unique_ptr<flutter::MethodChannel<EncodableValue>> channel_;

  // Private implementation.
};

// static
void VideoPlayerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar *registrar) {
  auto channel = std::make_unique<flutter::MethodChannel<EncodableValue>>(
      registrar->messenger(), kChannelName,
      &flutter::StandardMethodCodec::GetInstance());
  auto *channel_pointer = channel.get();

  // Uses new instead of make_unique due to private constructor.
  std::unique_ptr<VideoPlayerPlugin> plugin(
      new VideoPlayerPlugin(std::move(channel)));

  channel_pointer->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

VideoPlayerPlugin::VideoPlayerPlugin(
    std::unique_ptr<flutter::MethodChannel<EncodableValue>> channel)
    : channel_(std::move(channel)) {}

VideoPlayerPlugin::~VideoPlayerPlugin() {}

EncodableValue GrabEncodableValueFromArgs(const EncodableValue& arguments, const char* key) {
  EncodableMap arg_map = arguments.MapValue();
  auto it = arg_map.find(EncodableValue(key));
  if (it != arg_map.end()) {
    return it->second;
  }
  return EncodableValue();
}

void VideoPlayerPlugin::Create(const EncodableValue& arguments, FlutterResponder result) {
  EncodableValue uri = GrabEncodableValueFromArgs(arguments, "uri");
  if (uri.IsNull()) {
     uri = GrabEncodableValueFromArgs(arguments, "asset");
     if (uri.IsNull()) {
       result->Error("Asset arguments do not exist");
       return;
     }
  }
  string uri_val = uri.StringValue();
  

  result->Success();
}

void VideoPlayerPlugin::Play(const EncodableValue& arguments, FlutterResponder result) {
  result->Success();
}

void VideoPlayerPlugin::Pause(const EncodableValue& arguments, FlutterResponder result) {
  result->Success();
}

void VideoPlayerPlugin::Position(const EncodableValue& arguments, FlutterResponder result) {
  result->Success();
}

void VideoPlayerPlugin::Dispose(const EncodableValue& arguments, FlutterResponder result) {
  result->Success();
}

void VideoPlayerPlugin::HandleMethodCall(
    const flutter::MethodCall<EncodableValue> &method_call,
    FlutterResponder result) {
  if (!method_call.arguments() || method_call.arguments()->IsNull()) {
    result->Error("Bad Arguments", "Null arguments received");
    return;
  }

  string method_name = method_call.method_name();

  cout << "Method called: " << method_name << endl;
  if (method_name.compare(kInitMethod) == 0) {
    result->Success();
  } else if (method_name.compare(kCreateMethod) == 0) {
    Create(method_call.arguments(), std::move(result));
  } else if (method_name.compare(kPlayMethod) == 0) {
    Play(method_call.arguments(), std::move(result));
  } else if (method_name.compare(kPauseMethod) == 0) {
    Pause(method_call.arguments(), std::move(result));
  } else if (method_name.compare(kPositionMethod) == 0) {
    Position(method_call.arguments(), std::move(result));
  } else if (method_name.compare(kDisposeMethod) == 0) {
    Dispose(method_call.arguments(), std::move(result));
  } else {
    result->NotImplemented();
  }
}
}  // namespace plugins_video_player

void VideoPlayerPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  // The plugin registrar owns the plugin, registered callbacks, etc., so must
  // remain valid for the life of the application.
  static auto *plugin_registrar = new flutter::PluginRegistrarGlfw(registrar);
  plugins_video_player::VideoPlayerPlugin::RegisterWithRegistrar(
      plugin_registrar);
}
