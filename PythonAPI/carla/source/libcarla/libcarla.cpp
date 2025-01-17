// Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.
// 包含 CARLA 引擎相关的头文件
#include <carla/Memory.h>// 提供内存管理相关的功能（例如内存分配、智能指针等）
#include <carla/PythonUtil.h> // 提供与 Python 交互的工具和函数，通常用于在 C++ 和 Python 之间传递数据
#include <carla/Time.h> // 提供时间相关的功能，通常用于处理模拟时间、时间戳等
// 包含标准库相关的头文件
#include <ostream>// 提供输出流功能，通常用于处理与输出相关的操作（例如 std::cout）
#include <type_traits> // 提供类型特征（类型检查）功能，用于在编译时对类型进行推断或特性检查
#include <vector> // 提供动态数组容器类 std::vector，用于存储一组数据并提供高效的访问与操作

// 对于Python中的变量类型，Boost.Python都有相应的类对应，他们都是boost::python::object的子类。
// boost::python::object 包装了PyObject *, 通过这种方式，C++可以平滑的操作python对象。
// Boost.Python的主要目标既保持Python的编程风格同时又提供C++和Python的双向映射。
template <typename OptionalT>
static boost::python::object OptionalToPythonObject(OptionalT &optional) {
  return optional.has_value() ? boost::python::object(*optional) : boost::python::object();
}

// 方便地进行没有参数的请求。
// carla::PythonUtil::ReleaseGIL 文档：https://carla.org/Doxygen/html/d1/d0a/classcarla_1_1PythonUtil_1_1ReleaseGIL.html
#define CALL_WITHOUT_GIL(cls, fn) +[](cls &self) { \
      carla::PythonUtil::ReleaseGIL unlock; \
      return self.fn(); \
    }

// 方便地进行带有1个参数的请求。
// std::forward 主要用于完美转发，能够保留传递给函数参数的值类别（lvalue 或 rvalue），确保在转发参数时不丢失其原有的值性质。
#define CALL_WITHOUT_GIL_1(cls, fn, T1_) +[](cls &self, T1_ t1) { \
      carla::PythonUtil::ReleaseGIL unlock; \
      return self.fn(std::forward<T1_>(t1)); \
    }

// 方便地进行带有2个参数的请求。
#define CALL_WITHOUT_GIL_2(cls, fn, T1_, T2_) +[](cls &self, T1_ t1, T2_ t2) { \
      carla::PythonUtil::ReleaseGIL unlock; \
      return self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2)); \
    }

// 方便地进行带有3个参数的请求。
#define CALL_WITHOUT_GIL_3(cls, fn, T1_, T2_, T3_) +[](cls &self, T1_ t1, T2_ t2, T3_ t3) { \
      carla::PythonUtil::ReleaseGIL unlock; \
      return self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2), std::forward<T3_>(t3)); \
    }

// 方便地进行带有4个参数的请求。
#define CALL_WITHOUT_GIL_4(cls, fn, T1_, T2_, T3_, T4_) +[](cls &self, T1_ t1, T2_ t2, T3_ t3, T4_ t4) { \
      carla::PythonUtil::ReleaseGIL unlock; \
      return self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2), std::forward<T3_>(t3), std::forward<T4_>(t4)); \
    }

// 方便地进行带有5个参数的请求。
#define CALL_WITHOUT_GIL_5(cls, fn, T1_, T2_, T3_, T4_, T5_) +[](cls &self, T1_ t1, T2_ t2, T3_ t3, T4_ t4, T5_ t5) { \
      carla::PythonUtil::ReleaseGIL unlock; \
      return self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2), std::forward<T3_>(t3), std::forward<T4_>(t4), std::forward<T5_>(t5)); \
    }

// 方便地进行没有参数的常量请求。
#define CONST_CALL_WITHOUT_GIL(cls, fn) CALL_WITHOUT_GIL(const cls, fn)
#define CONST_CALL_WITHOUT_GIL_1(cls, fn, T1_) CALL_WITHOUT_GIL_1(const cls, fn, T1_)
#define CONST_CALL_WITHOUT_GIL_2(cls, fn, T1_, T2_) CALL_WITHOUT_GIL_2(const cls, fn, T1_, T2_)
#define CONST_CALL_WITHOUT_GIL_3(cls, fn, T1_, T2_, T3_) CALL_WITHOUT_GIL_3(const cls, fn, T1_, T2_, T3_)
#define CONST_CALL_WITHOUT_GIL_4(cls, fn, T1_, T2_, T3_, T4_) CALL_WITHOUT_GIL_4(const cls, fn, T1_, T2_, T3_, T4_)

// 方便用于需要复制返回值的const请求。 
#define CALL_RETURNING_COPY(cls, fn) +[](const cls &self) \
        -> std::decay_t<std::result_of_t<decltype(&cls::fn)(cls*)>> { \
      return self.fn(); \
    }

// 方便用于需要复制返回值的const请求。
#define CALL_RETURNING_COPY_1(cls, fn, T1_) +[](const cls &self, T1_ t1) \
        -> std::decay_t<std::result_of_t<decltype(&cls::fn)(cls*, T1_)>> { \
      return self.fn(std::forward<T1_>(t1)); \
    }

template<typename T>
std::vector<T> PythonLitstToVector(boost::python::list &input) {
  std::vector<T> result;
  boost::python::ssize_t list_size = boost::python::len(input);
  for (boost::python::ssize_t i = 0; i < list_size; ++i) {
    result.emplace_back(boost::python::extract<T>(input[i]));
  }
  return result;
}

// 方便地需要将返回值转换为Python列表的const请求。
#define CALL_RETURNING_LIST(cls, fn) +[](const cls &self) { \
      boost::python::list result; \
      for (auto &&item : self.fn()) { \
        result.append(item); \
      } \
      return result; \
    }

// 方便需要将返回值转换为Python列表的const请求。
#define CALL_RETURNING_LIST_1(cls, fn, T1_) +[](const cls &self, T1_ t1) { \
      boost::python::list result; \
      for (auto &&item : self.fn(std::forward<T1_>(t1))) { \
        result.append(item); \
      } \
      return result; \
    }

#define CALL_RETURNING_LIST_2(cls, fn, T1_, T2_) +[](const cls &self, T1_ t1, T2_ t2) { \
      boost::python::list result; \
      for (auto &&item : self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2))) { \
        result.append(item); \
      } \
      return result; \
    }

#define CALL_RETURNING_LIST_3(cls, fn, T1_, T2_, T3_) +[](const cls &self, T1_ t1, T2_ t2, T3_ t3) { \
      boost::python::list result; \
      for (auto &&item : self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2), std::forward<T3_>(t3))) { \
        result.append(item); \
      } \
      return result; \
    }

#define CALL_RETURNING_OPTIONAL(cls, fn) +[](const cls &self) { \
      auto optional = self.fn(); \
      return OptionalToPythonObject(optional); \
    }

#define CALL_RETURNING_OPTIONAL_1(cls, fn, T1_) +[](const cls &self, T1_ t1) { \
      auto optional = self.fn(std::forward<T1_>(t1)); \
      return OptionalToPythonObject(optional); \
    }

#define CALL_RETURNING_OPTIONAL_2(cls, fn, T1_, T2_) +[](const cls &self, T1_ t1, T2_ t2) { \
      auto optional = self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2)); \
      return OptionalToPythonObject(optional); \
    }

#define CALL_RETURNING_OPTIONAL_3(cls, fn, T1_, T2_, T3_) +[](const cls &self, T1_ t1, T2_ t2, T3_ t3) { \
      auto optional = self.fn(std::forward<T1_>(t1), std::forward<T2_>(t2), std::forward<T3_>(t3)); \
      return OptionalToPythonObject(optional); \
    }

#define CALL_RETURNING_OPTIONAL_WITHOUT_GIL(cls, fn) +[](const cls &self) { \
      auto call = CONST_CALL_WITHOUT_GIL(cls, fn); \
      auto optional = call(self); \
      return optional.has_value() ? boost::python::object(*optional) : boost::python::object(); \
    }

template <typename T>
static void PrintListItem_(std::ostream &out, const T &item) {
  out << item;
}

template <typename T>
static void PrintListItem_(std::ostream &out, const carla::SharedPtr<T> &item) {
  if (item == nullptr) {
    out << "nullptr";
  } else {
    out << *item;
  }
}

template <typename Iterable>
static std::ostream &PrintList(std::ostream &out, const Iterable &list) {
  out << '[';
  if (!list.empty()) {
    auto it = list.begin();
    PrintListItem_(out, *it);
    for (++it; it != list.end(); ++it) {
      out << ", ";
      PrintListItem_(out, *it);
    }
  }
  out << ']';
  return out;
}

namespace std {

  template <typename T>
  std::ostream &operator<<(std::ostream &out, const std::vector<T> &vector_of_stuff) {
    return PrintList(out, vector_of_stuff);
  }

  template <typename T, typename H>
  std::ostream &operator<<(std::ostream &out, const std::pair<T,H> &data) {
    out << "(" << data.first << "," << data.second << ")";
    return out;
  }

} // namespace std

static carla::time_duration TimeDurationFromSeconds(double seconds) {
  size_t ms = static_cast<size_t>(1e3 * seconds);
  return carla::time_duration::milliseconds(ms);
}

static auto MakeCallback(boost::python::object callback) {
  namespace py = boost::python;
  // 确保回调实际上是可调用的。
  if (!PyCallable_Check(callback.ptr())) {
    PyErr_SetString(PyExc_TypeError, "callback argument must be callable!");
    py::throw_error_already_set();
  }

  // 我们需要在持有GIL的同时删除回调。
  using Deleter = carla::PythonUtil::AcquireGILDeleter;
  auto callback_ptr = carla::SharedPtr<py::object>{new py::object(callback), Deleter()};

  // 做一个lambda回调。
  return [callback=std::move(callback_ptr)](auto message) {
    carla::PythonUtil::AcquireGIL lock;
    try {
      py::call<void>(callback->ptr(), py::object(message));
    } catch (const py::error_already_set &) {
      PyErr_Print();
    }
  };
}

#include "V2XData.cpp"
#include "Geom.cpp"
#include "Actor.cpp"
#include "Blueprint.cpp"
#include "Client.cpp"
#include "Control.cpp"
#include "Exception.cpp"
#include "Map.cpp"
#include "Sensor.cpp"
#include "SensorData.cpp"
#include "Snapshot.cpp"
#include "Weather.cpp"
#include "World.cpp"
#include "Commands.cpp"
#include "TrafficManager.cpp"
#include "LightManager.cpp"
#include "OSM2ODR.cpp"

#ifdef LIBCARLA_RSS_ENABLED
#include "AdRss.cpp"
#endif

BOOST_PYTHON_MODULE(libcarla) {
  using namespace boost::python;
#if PY_MAJOR_VERSION < 3 || PY_MINOR_VERSION < 7
  PyEval_InitThreads();
#endif
  // 设置 Python 模块的路径
  scope().attr("__path__") = "libcarla";// 将 "libcarla" 作为 Python 模块的路径。这表示该模块的代码文件将位于 `libcarla` 目录中。
  // 导出几种不同类型的数据和功能
  export_geom();// 导出几何相关的功能，可能涉及 3D 模型、物体形状等几何数据的导出
  export_control();// 导出控制相关的功能，可能涉及自动驾驶或仿真环境中的控制接口
  export_blueprint(); // 导出蓝图功能，通常用于定义车辆、行人等对象的构建蓝图或模型
  export_actor(); // 导出 Actor（演员）相关功能，Actor 通常指仿真世界中的动态实体，如车辆、行人等
  export_sensor();// 导出传感器相关功能，用于处理与传感器（如摄像头、雷达、激光雷达等）相关的操作
  export_sensor_data();// 导出传感器数据功能，可能用于获取和处理传感器采集到的数据
  export_snapshot();// 导出快照功能，用于保存和恢复仿真状态或世界状态的快照
  export_weather();// 导出天气相关功能，可能用于仿真中的天气控制（如晴天、雨天、雾霾等）
  export_world(); // 导出世界（World）功能，通常涉及到整个仿真环境的管理和控制
  export_map();// 导出地图功能，可能涉及获取地图数据或在仿真中加载和使用地图
  export_client();// 导出客户端相关功能，通常用于客户端与仿真服务之间的通信
  export_exception();// 导出异常处理功能，用于在程序运行过程中捕获并处理错误
  export_commands();// 导出命令功能，可能用于执行仿真环境中的特定指令或操作
  export_trafficmanager();// 导出交通管理器功能，用于控制和管理仿真中的交通流
  export_lightmanager();// 导出光照管理器功能，可能涉及控制和调整仿真环境中的光照（如日夜变化）
  #ifdef LIBCARLA_RSS_ENABLED
  export_ad_rss();
  #endif
  export_osm2odr();
}
