#include "devices.h"
#include "mmu.h"
#include <stdexcept>
#include <sys/mman.h>

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

mmio_device_map_t& mmio_device_map()
{
  static mmio_device_map_t device_map;
  return device_map;
}

static auto empty_device = rom_device_t(std::vector<char>());

bus_t::bus_t()
  : bus_t(&empty_device)
{
}

bus_t::bus_t(abstract_device_t* fallback)
  : fallback(fallback)
{
}

void bus_t::add_device(reg_t addr, abstract_device_t* dev)
{
  // Allow empty devices by omitting them
  auto size = dev->size();
  if (size == 0)
    return;

  // Reject devices that overflow address size
  if (addr + size - 1 < addr) {
    fprintf(stderr, "device at [%" PRIx64 ", %" PRIx64 ") overflows address size\n",
            addr, addr + size);
    abort();
  }

  // Reject devices that overlap other devices
  if (auto it = devices.upper_bound(addr);
      (it != devices.end() && addr + size - 1 >= it->first) ||
      (it != devices.begin() && (it--, it->first + it->second->size() - 1 >= addr))) {
    fprintf(stderr, "devices at [%" PRIx64 ", %" PRIx64 ") and [%" PRIx64 ", %" PRIx64 ") overlap\n",
            it->first, it->first + it->second->size(), addr, addr + size);
    abort();
  }

  devices[addr] = dev;
}

bool bus_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  if (auto [base, dev] = find_device(addr, len); dev)
    return dev->load(addr - base, len, bytes);
  return false;
}

bool bus_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  if (auto [base, dev] = find_device(addr, len); dev)
    return dev->store(addr - base, len, bytes);
  return false;
}

reg_t bus_t::size()
{
  if (auto last = devices.rbegin(); last != devices.rend())
    return last->first + last->second->size();
  return 0;
}

std::pair<reg_t, abstract_device_t*> bus_t::find_device(reg_t addr, size_t len)
{
  if (unlikely(!len || addr + len - 1 < addr))
    return std::make_pair(0, nullptr);

  // Obtain iterator to device immediately after the one that might match
  auto it_after = devices.upper_bound(addr);
  reg_t base, size;
  if (likely(it_after != devices.begin())) {
    // Obtain iterator to device that might match
    auto it = std::prev(it_after);
    base = it->first;
    size = it->second->size();
    if (likely(addr - base + len - 1 < size)) {
      // it fully contains [addr, addr + len)
      return std::make_pair(it->first, it->second);
    }
  }

  if (unlikely((it_after != devices.end() && addr + len - 1 >= it_after->first)
      || (it_after != devices.begin() && addr - base < size))) {
    // it_after or it contains part of, but not all of, [addr, add + len)
    return std::make_pair(0, nullptr);
  }

  // No matching device
  return std::make_pair(0, fallback);
}

const std::map<reg_t, abstract_device_t*>& bus_t::get_devices() const {
    return devices;
}

mem_t::mem_t(reg_t size)
  : chunks((size + CHUNK_SIZE - 1) / CHUNK_SIZE, nullptr), sz(size)
{
  if (size == 0 || size % PGSIZE != 0)
    throw std::runtime_error("memory size must be a positive multiple of 4 KiB");
}

mem_t::~mem_t()
{
  for (size_t i = 0; i < chunks.size(); i++)
    if (chunks[i])
      munmap(chunks[i], chunk_size(i));
}

bool mem_t::load_store(reg_t addr, size_t len, uint8_t* bytes, bool store)
{
  if (addr + len < addr || addr + len > sz)
    return false;

  while (len > 0) {
    auto n = std::min(PGSIZE - (addr % PGSIZE), reg_t(len));

    if (store)
      memcpy(this->contents(addr), bytes, n);
    else
      memcpy(bytes, this->contents(addr), n);

    addr += n;
    bytes += n;
    len -= n;
  }

  return true;
}

char* mem_t::contents(reg_t addr) {
  char*& chunk = chunks.at(addr / CHUNK_SIZE);

  if (unlikely(chunk == nullptr)) {
    // MAP_NORESERVE because target memory is sparse by design: only the pages
    // the target actually touches ever get backed by host memory.
    void* p = mmap(nullptr, chunk_size(addr / CHUNK_SIZE), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED)
      throw std::bad_alloc();
    chunk = (char*)p;
  }

  return chunk + addr % CHUNK_SIZE;
}

void mem_t::dump(std::ostream& o) {
  const std::vector<char> empty(CHUNK_SIZE, 0);
  for (size_t i = 0; i < chunks.size(); i++)
    o.write(chunks[i] ? chunks[i] : empty.data(), chunk_size(i));
}

external_sim_device_t::external_sim_device_t(abstract_sim_if_t* sim) 
  : external_simulator(sim) {}

void external_sim_device_t::set_simulator(abstract_sim_if_t* sim) {
  external_simulator = sim;
}

bool external_sim_device_t::load(reg_t addr, size_t len, uint8_t* bytes) {
  if (unlikely(external_simulator == nullptr)) return false;
  return external_simulator->load(addr, len, bytes);
}

bool external_sim_device_t::store(reg_t addr, size_t len, const uint8_t* bytes) {
  if (unlikely(external_simulator == nullptr)) return false;
  return external_simulator->store(addr, len, bytes);
}

reg_t external_sim_device_t::size() {
  if (unlikely(external_simulator == nullptr)) return 0;
  return PGSIZE; // TODO: proper size
}
