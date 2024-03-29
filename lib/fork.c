// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	//виртуальный адрес, который вызвал ошибку
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 9: Your code here.
	pte_t pte = uvpt[PGNUM(addr)];
    if (!(err & FEC_WR) || !(pte & PTE_COW))
    	// проверяет, что ошибка является записью (проверка на FEC_WR в коде ошибки) 
    	//и что PTE для страницы помечен PTE_COW. Если нет, паникует
		panic("access");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 9: Your code here.
	//выделяет новую страницу, отображаемую во временном месте 
	if (sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P))
        	panic("alloc");
	addr = ROUNDDOWN(addr, PGSIZE);
	//копирует содержимое вызвавшей ошибку страницы в нее
    memmove(PFTEMP, addr, PGSIZE);
    //Затем обработчик ошибок отображает новую страницу на соответствующий адрес с разрешениями 
    //на чтение/запись вместо старого отображения только для чтения.
	if (sys_page_map(0, PFTEMP, 0, addr, PTE_W | PTE_U | PTE_P))
       		panic("map");
    //удаляет отображение по PFTEMP
	if (sys_page_unmap(0, PFTEMP))
    		panic("unmap");
	return;
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	// LAB 9: Your code here.
	pte_t pte = uvpt[pn];
	void *va = (void *)(pn << PGSHIFT);
	if (pte & PTE_SHARE) {
		//если у записи в таблице страниц установлен этот бит, 
		//данная запись должна быть скопирована непосредственно от родительского процесса к дочернему
         if (sys_page_map(0, va, envid, va, pte & PTE_SYSCALL))
             panic("duppage share");
    } 
    else 
      if ((pte & PTE_W) || (pte & PTE_COW)) {
          if (sys_page_map(0, va, envid, va, PTE_COW | PTE_U | PTE_P))
              panic("duppage cow");
          if (sys_page_map(0, va, 0, va, PTE_COW | PTE_U | PTE_P))
              panic("duppage perm");
 	  } 
 	else 
	  if (sys_page_map(0, va, envid, va, PTE_U | PTE_P))
	      panic("duppage map");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 9: Your code here.
	//Родитель устанавливает pgfault() в качестве обработчика ошибок страницы C-уровня
	set_pgfault_handler(pgfault);
	//Родитель вызывает sys_exofork(), чтобы создать дочерний процесс
	envid_t envid = sys_exofork();
	if (envid < 0) {
	    panic("fork");
	}
	else
		if (envid == 0) {
			//если первый процесс
		    thisenv = &envs[ENVX(sys_getenvid())];
        	return 0;
		}
	int ipd, ipt;
	for (ipd = 0; ipd != PDX(UTOP); ++ipd) {
		//Для каждой записываемой или copy-on-write-страницы 
		//в своем адресном пространстве ниже UTOP родитель вызывает duppage
	    if (!(uvpd[ipd] & PTE_P))
      		continue;
        for (ipt = 0; ipt != NPTENTRIES; ++ipt) {
        	// NPTENTRIES : page table entries per page table
	      	unsigned pn = (ipd << 10) | ipt;
		    if (pn == PGNUM(UXSTACKTOP - PGSIZE)) {
	        	if (sys_page_alloc(envid,(void *)(UXSTACKTOP - PGSIZE),PTE_W | PTE_U | PTE_P))
		 	        panic("fork: sys_page_alloc\n");
		        continue;
	        }
	        if (uvpt[pn] & PTE_P)
        	   	duppage(envid, pn);
        }
    }
    //Родительский процесс устанавливает такую же, как у себя, 
    //точку входа в обработчик ошибки страницы в дочернем процессе
  	if (sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall))
        panic("pgfault upcall");
    //Дочерний процесс в настоящее время готов работать, так что родитель помечает его runnable
	if (sys_env_set_status(envid, ENV_RUNNABLE))
	    panic("env status");
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
