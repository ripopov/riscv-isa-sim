require_extension('S');
require(p->has_mmu());
if (STATE.v) {
  if (STATE.prv == PRV_U || get_field(STATE.hstatus->read(), HSTATUS_VTVM))
    require_novirt();
} else {
  require_privilege(get_field(STATE.mstatus->read(), MSTATUS_TVM) ? PRV_M : PRV_S);
}
if (insn.rs1() == 0)
  MMU.flush_tlb();
else
  MMU.flush_tlb_vaddr(RS1);
