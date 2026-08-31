require_extension('S');
require(p->has_mmu());
if (STATE.v) {
  if (STATE.prv == PRV_U || get_field(STATE.hstatus->read(), HSTATUS_VTVM))
    require_novirt();
} else {
  require_privilege(get_field(STATE.mstatus->read(), MSTATUS_TVM) ? PRV_M : PRV_S);
}
// rs1 = x0 orders translations for all virtual addresses; otherwise only those
// for the address in rs1.  rs2 names an ASID, which this MMU does not track, so
// its translations are invalidated whatever the ASID -- always permitted.
if (insn.rs1() == 0)
  MMU.flush_tlb();
else
  MMU.flush_tlb_vaddr(RS1);
