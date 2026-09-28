#pragma once

// Development-only render validation, driven from Scripts/CheckRendering.ps1.
//
// When the process is started with -F101RenderCheck it waits for shader compilation to finish,
// samples frame/GPU times, logs the actual RHI / ray tracing / Lumen state as
// "F101RenderCheck: key=value" lines, triggers ProfileGPU and then requests engine exit.
// It only reads engine state; it never changes rendering settings.
namespace Floor101::RenderCheck
{
	void StartIfRequested();
	void Stop();
}
