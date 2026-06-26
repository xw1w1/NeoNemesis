#include <Windows.h>

#include "omni/syscall.hpp"

#include <cstdint>
#include <print>

struct process_basic_information {
  void* reserved1{};
  void* peb_base_address{};
  void* reserved2[2]{};
  std::uintptr_t unique_process_id{};
  void* reserved3{};
};

using nt_query_info_process_fn = omni::status (*)(HANDLE, ULONG, void*, ULONG, ULONG*);

int main() {
  omni::inline_syscaller<nt_query_info_process_fn> query_process{"NtQueryInformationProcess"};

  process_basic_information process_info{};
  ULONG return_length{};

  auto query_status = query_process.try_invoke(::GetCurrentProcess(), 0U, &process_info, sizeof(process_info), &return_length);
  if (!query_status) {
    std::println("Failed to resolve NtQueryInformationProcess: {}", query_status.error().message());
    return 1;
  }

  process_basic_information free_typed_info{};
  process_basic_information free_generic_info{};
  ULONG free_typed_return_length{};
  ULONG free_generic_return_length{};

  omni::status free_typed_status = omni::inline_syscall<nt_query_info_process_fn>("NtQueryInformationProcess",
    ::GetCurrentProcess(),
    0U,
    &free_typed_info,
    sizeof(free_typed_info),
    &free_typed_return_length);

  auto free_generic_status = omni::inline_syscall<omni::status>("NtQueryInformationProcess",
    ::GetCurrentProcess(),
    0U,
    &free_generic_info,
    sizeof(free_generic_info),
    &free_generic_return_length);

  std::println("Typed inline syscall wrapper around NtQueryInformationProcess:");
  std::println("  status               : 0x{:08X}", static_cast<std::uint32_t>(query_status->value));
  std::println("  success              : {}", query_status->is_success());
  std::println("  PEB                  : {:#x}", reinterpret_cast<std::uintptr_t>(process_info.peb_base_address));
  std::println("  process id           : {}", process_info.unique_process_id);
  std::println("  return length        : {}", return_length);

  std::println();

  std::println("Free inline syscall overloads:");
  std::println("  typed status         : 0x{:08X}", static_cast<std::uint32_t>(free_typed_status.value));
  std::println("  generic status       : 0x{:08X}", static_cast<std::uint32_t>(free_generic_status.value));
  std::println("  same PEB             : {}", free_typed_info.peb_base_address == process_info.peb_base_address);
  std::println("  same process id      : {}", free_typed_info.unique_process_id == process_info.unique_process_id);
  std::println("  generic same PEB     : {}", free_generic_info.peb_base_address == process_info.peb_base_address);
  std::println("  generic same pid     : {}", free_generic_info.unique_process_id == process_info.unique_process_id);
  std::println("  typed return length  : {}", free_typed_return_length);
  std::println("  generic return length: {}", free_generic_return_length);

  auto yield_status = omni::inline_syscall<omni::status>("NtYieldExecution");

  std::println();

  std::println("Generic inline syscall overload:");
  std::println("  NtYieldExecution     : 0x{:08X}", static_cast<std::uint32_t>(yield_status.value));
  std::println("  success              : {}", yield_status.is_success());
}
