"""Compile the production polling methods against fake memory/events, before and after.

This tests notification logic, not OS scheduling or gameplay. Run with a configured
clang-cl toolchain: python Test-SyncpointNotify.py OUTPUT_DIRECTORY
"""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
path = 'src/core/hle/service/nx2_gsv.h'
before = subprocess.check_output(['git', '-C', str(root), 'show', '1f00d28f4a:' + path], text=True)
after = (root / path).read_text()

def method(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
using u32 = uint32_t; using u64 = uint64_t;
struct Memory { std::map<u64,u64> words; u64 Read64(u64 a){return words[a];}
void Write64(u64 a,u64 v){words[a]=v;} };
struct Manager {std::array<u32,192> values{}; u32 GetGuestSyncpointValue(u32 i){return values.at(i);} };
struct Host {Manager manager; Manager& GetSyncpointManager(){return manager;} };
struct System {Host host; Host& Host1x(){return host;} int Kernel(){return 0;} };
struct Event {int count=0; void Signal(int){++count;} };
struct Session {
System system; Memory mem; Memory* m_syncpt_memory=&mem; Event notify;
std::array<Event*,2> m_syncpt_events{nullptr,&notify};
static constexpr u32 HostSyncpoints=192, SyncptValuesOffset=0x40,
SyncptRwBitsOffset=0x100, SyncptRoBitsOffset=0x1100,
SyncptRoSummaryOffset=0x1040, SyncptRwSummaryOffset=0;
u64 m_syncpt_ro=0x10000,m_syncpt_rw=0x20000; u32 m_syncpt_count=192;
std::array<u32,HostSyncpoints> m_syncpt_polled_values{};
'''
suffix = r'''
void Arm(u32 id){u64 bit=u64{1}<<(id%64); auto w=id/64;
mem.Write64(m_syncpt_rw+SyncptRwBitsOffset+w*8,bit);
mem.Write64(m_syncpt_ro+SyncptRoBitsOffset+w*8,bit);}
};
int main(){
// Rearm without changing the armed bit, including word boundaries and wraparound.
for (u32 id: {0u,63u,64u,127u,128u,191u}) {
 Session s; s.Arm(id); s.UpdateSyncptValues();
 s.system.host.manager.values[id]=1; s.PollSyncpts(); if(s.notify.count!=1)return 1;
 s.PollSyncpts(); if(s.notify.count!=1)return 2;
 s.system.host.manager.values[id]=UINT32_MAX; s.PollSyncpts();
 s.system.host.manager.values[id]=0; s.PollSyncpts(); if(s.notify.count!=3)return 3;
}
// An already mirrored advance still notifies; unarmed advances do not.
{Session s; s.Arm(64); s.UpdateSyncptValues(); s.system.host.manager.values[64]=7;
 s.mem.Write64(s.m_syncpt_ro+s.SyncptValuesOffset+64*8,7);
 s.PollSyncpts(); if(s.notify.count!=1)return 4;}
{Session s; s.system.host.manager.values[4]=9; s.PollSyncpts(); if(s.notify.count)return 5;}
// Preserve ordinary acknowledgement, summary copying and no-memory behavior.
{Session s; s.mem.Write64(s.m_syncpt_rw+s.SyncptRwBitsOffset,8);
 s.mem.Write64(s.m_syncpt_rw+s.SyncptRwSummaryOffset,77); s.PollSyncpts();
 if(s.notify.count!=1 || s.mem.Read64(s.m_syncpt_ro+s.SyncptRoBitsOffset)!=8 ||
 s.mem.Read64(s.m_syncpt_ro+s.SyncptRoSummaryOffset)!=77)return 6;}
{Session s; s.m_syncpt_memory=nullptr; s.PollSyncpts(); if(s.notify.count)return 7;}
return 0;}
'''
for label, source in [('before',before),('after',after)]:
    cpp=out / (label+'.cpp'); exe=out / (label+'.exe')
    cpp.write_text(prefix+method(source,'bool UpdateSyncptValues(')+method(source,'void PollSyncpts()')+suffix)
    subprocess.run(['clang-cl','/nologo','/std:c++20','/EHsc',str(cpp),'/Fe:'+str(exe),'/Fo:'+str(out/(label+'.obj'))],check=True)
    result=subprocess.run([str(exe)]).returncode
    expected=1 if label=='before' else 0
    if result!=expected: raise RuntimeError(f'{label}: expected {expected}, got {result}')
    print(f'{label}: exit {result}, expected {expected}')
print('PASS: production polling regression fails before and passes after; mock dependencies only.')
