.PHONY: core-test core-clean phase5-check theme-font-smoke

core-test:
	$(MAKE) -C core/core_base test
	$(MAKE) -C core/core_io test
	$(MAKE) -C core/core_data test
	$(MAKE) -C core/core_math test
	$(MAKE) -C core/core_theme test
	$(MAKE) -C core/core_font test
	$(MAKE) -C core/core_space test
	$(MAKE) -C core/core_scene test
	$(MAKE) -C core/core_pack test
	$(MAKE) -C core/core_memdb test
	$(MAKE) -C core/core_trace test
	$(MAKE) -C core/core_time test
	$(MAKE) -C core/core_queue test
	$(MAKE) -C core/core_sched test
	$(MAKE) -C core/core_jobs test
	$(MAKE) -C core/core_workers test
	$(MAKE) -C core/core_wake test
	$(MAKE) -C core/core_kernel test
	$(MAKE) -C kit/kit_viz test

core-clean:
	$(MAKE) -C core/core_base clean
	$(MAKE) -C core/core_io clean
	$(MAKE) -C core/core_data clean
	$(MAKE) -C core/core_math clean
	$(MAKE) -C core/core_theme clean
	$(MAKE) -C core/core_font clean
	$(MAKE) -C core/core_space clean
	$(MAKE) -C core/core_scene clean
	$(MAKE) -C core/core_pack clean
	$(MAKE) -C core/core_memdb clean
	$(MAKE) -C core/core_trace clean
	$(MAKE) -C core/core_time clean
	$(MAKE) -C core/core_queue clean
	$(MAKE) -C core/core_sched clean
	$(MAKE) -C core/core_jobs clean
	$(MAKE) -C core/core_workers clean
	$(MAKE) -C core/core_wake clean
	$(MAKE) -C core/core_kernel clean
	$(MAKE) -C kit/kit_viz clean

phase5-check:
	$(MAKE) core-test
	$(MAKE) -C ../daw
	$(MAKE) -C ../datalab
	$(MAKE) -C ../datalab test
	../datalab/datalab --pack core/core_pack/tests/fixtures/physics_v1_sample.pack --no-gui
	../datalab/datalab --pack core/core_pack/tests/fixtures/daw_v1_sample.pack --no-gui

theme-font-smoke:
	./scripts/run_theme_font_preset_smoke.sh
