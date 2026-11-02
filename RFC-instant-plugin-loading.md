# RFC — Instant Plugin Loading (EVE)

---

## Problem Statement

EVE as plugin hosted in END works. Terminal run. The issue is loading plugin is REALLY SLOW. It may
take 2-3 second from pane created then terminal running. While it's loading, it's blocking message
thread thus mac showing spinning wheel for couple of second. This is very bad UX.

Compare to jreng-filter-strip with lots images, EVE is only TextEditor with couple of lines of
active prompt to render. JFS loaded on DAW in subsecond. it's basically instaneous.

Because the timing issue, there's also bug surface in observations when terminal started. This is
arbitrary, not always happen but consistent pattern:

- sometimes first line of TextEditor above active prompt prints `%`
- sometimes first block of TextEditor above active prmopt prints OMP help. OMP on ARCHITECT config
  is being called to print the active prompt. This is smell either parsing, or timing issue since
  environment, shell integration, config and cli disagree. hence OMP thinks there's error and prints
  its help page.

---

## Logical Steps

[] using `scout` profile every single point, from pane being created, plugin creation, plugin
loading, until terminal running, this possibly profiling both side at end and might also at eve. but
end as host is the priority

[] the total time result from scout as baseline, to find each bottleneck to eliminate

[] pinpoint each bottleneck time, step by step find the most BLESSED compliance approach to
eliminate all bottleneck down to lowest number, FASTEST TIME [] the achitecture of audio (terminal
processor) thread always lock free, but if there's any bottleneck especially on message thread could
be eliminated with parallel job to shave time is preferable.  [] the acceptable total time from new
pane created until terminal running is less than 500 ms, or LESS

---

## Key points

JFS loaded in subsecond in DAW as fact, proven there is nothing wrong with plugin_bootstrap. It was already measured and optimised. We have all stack end-to-end, EVE is our plugin, END is our host. we have full control. We dont want hack, we dont want shortcut, we want CORRECTNESS, adherence to all crossplatfrom, cross binary, one SSOT method.

- why message thread blocked when loading?
- is there any unnecessary allocation? 
- is there any unnecessary copying?
- is there any unnecessary iteration for finding anything that might failed? note that EVE loading is DETERMINISTIC, the binary, the plugin, the instance all paramters are KNOWN at build time
- is there unnecessary waiting? that could be done in parallel? 
