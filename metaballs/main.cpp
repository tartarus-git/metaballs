#define WINDOW_TITLE "Metaballs"
#define WINDOW_CLASS_NAME "METABALLS_WINDOW"
#include "windowSetup.h"

#include "cl_bindings_and_helpers.h"

#include <vector>

#include "debugOutput.h"

#define UWM_EXIT_FROM_THREAD WM_USER

bool isAlive = true;

unsigned int windowMouseX;
unsigned int windowMouseY;
bool mouseUpdated = false;

LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_MOUSEMOVE:
		if (mouseUpdated) { return 0; }
		windowMouseX = LOWORD(lParam);
		windowMouseY = HIWORD(lParam);
		mouseUpdated = true;
		return 0;
	case WM_DESTROY:
		isAlive = false;
		graphicsThread.join();
		PostQuitMessage(EXIT_SUCCESS);
		return 0;
	case UWM_EXIT_FROM_THREAD:
		graphicsThread.join();
		PostQuitMessage(EXIT_FAILURE);	// UWM_EXIT_FROM_THREAD is used only for bugs, so make sure the exit code is set to EXIT_FAILURE.
		return 0;
	default: if (listenForResize(uMsg, wParam, lParam)) { return 0; }
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

unsigned int windowWidth;
unsigned int windowHeight;

unsigned int newWindowWidth;
unsigned int newWindowHeight;
bool windowResized = false;
void setWindowSize(unsigned int newWindowWidth, unsigned int newWindowHeight) {								// This gets triggered once if the first action of you do is to move the window, for the rest of the moves, it doesn't get triggered.
	::newWindowWidth = newWindowWidth;																		// This is practically unavoidable without a little much effort. It's not really bad as long as it's just one time, so I'm going to leave it.
	::newWindowHeight = newWindowHeight;
	windowResized = true;
}

cl_platform_id computePlatform;
cl_context computeContext;
cl_device_id computeDevice;
cl_command_queue computeCommandQueue;

bool releaseComputeContextVars() {
	cl_int err = clReleaseCommandQueue(computeCommandQueue);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to release computeCommandQueue" << debuglogger::endl;
		return true;
	}
	err = clReleaseContext(computeContext);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to release computeContext" << debuglogger::endl;
		return true;
	}
	return false;
}

cl_program computeProgram;
cl_kernel computeKernel;
size_t computeKernelWorkGroupSize;

bool releaseComputeKernelVars() {
	cl_int err = clReleaseKernel(computeKernel);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to release compute kernel" << debuglogger::endl;
		return true;
	}
	err = clReleaseProgram(computeProgram);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to release compute program" << debuglogger::endl;
		return true;
	}
	return false;
}

cl_mem outputFrame_computeImage;
cl_mem positions_computeBuffer;

bool releaseComputeMemoryObjects() {
	cl_int err = clReleaseMemObject(outputFrame_computeImage);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to release outputFrame_computeImage" << debuglogger::endl;
		return true;
	}
	err = clReleaseMemObject(positions_computeBuffer);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to release positions_computeBuffer" << debuglogger::endl;
		return true;
	}
	return false;
}

bool setupComputeDevice() {
	cl_int err = initOpenCLBindings();
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to initialize OpenCL bindings" << debuglogger::endl;
		return true;
	}

	err = initOpenCLVarsForBestDevice({ 2, 1 }, computePlatform, computeDevice, computeContext, computeCommandQueue);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to find the optimal compute device" << debuglogger::endl;
		if (!freeOpenCLLib()) { debuglogger::out << debuglogger::error << "failed to free OpenCL library" << debuglogger::endl; }
		return true;
	}

	std::string buildLog;
	err = setupComputeKernelFromFile(computeContext, computeDevice, "metaballRenderer.cl", "metaballRenderer", computeProgram, computeKernel, computeKernelWorkGroupSize, buildLog);				// TODO: Make sure this is in a released state when it fails as a qaruantee.
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set up compute kernel" << debuglogger::endl;
		if (err == CL_EXT_BUILD_FAILED_WITH_BUILD_LOG) {
			debuglogger::out << debuglogger::error << "failed build" << debuglogger::endl;
			debuglogger::out << "BUILD LOG:" << debuglogger::endl << buildLog << debuglogger::endl;
		}
		if (releaseComputeContextVars()) { debuglogger::out << debuglogger::error << "failed to free compute context vars" << debuglogger::endl; }
		if (!freeOpenCLLib()) { debuglogger::out << debuglogger::error << "failed to free OpenCL library" << debuglogger::endl; }
		return true;
	}

	return false;
}

bool setFrameKernelArg() {
	cl_int err = clSetKernelArg(computeKernel, 2, sizeof(cl_mem), &outputFrame_computeImage);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set outputFrame_computeImage kernel arg" << debuglogger::endl;
		return true;
	}
	return false;
}

bool setDefaultKernelArgs() {
	cl_int err = clSetKernelArg(computeKernel, 0, sizeof(cl_mem), &positions_computeBuffer);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set positions_computeBuffer kernel arg" << debuglogger::endl;
		return true;
	}
	return setFrameKernelArg();
}

const cl_image_format computeImageFormat = { CL_RGBA, CL_UNSIGNED_INT8 };
bool allocateFrameComputeBuffer() {
	cl_int err;
	outputFrame_computeImage = clCreateImage2D(computeContext, CL_MEM_WRITE_ONLY, &computeImageFormat, windowWidth, windowHeight, 0, nullptr, &err);
	if (!outputFrame_computeImage) {
		debuglogger::out << debuglogger::error << "failed to allocate outputFrame_computeImage" << debuglogger::endl;
		return true;
	}
	return false;
}

struct Position { int x; int y; };

std::vector<Position> positions;
bool allocateComputeBuffers() {
	if (allocateFrameComputeBuffer()) { return true; }

	cl_int err;
	positions_computeBuffer = clCreateBuffer(computeContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, positions.size() * sizeof(Position), positions.data(), &err);
	if (!positions_computeBuffer) {
		debuglogger::out << debuglogger::error << "failed to allocate positions_computeBuffer" << debuglogger::endl;
		return true;
	}

	return setDefaultKernelArgs();
}

bool updatePositionsComputeBuffer() {
	cl_int err = clEnqueueWriteBuffer(computeCommandQueue, positions_computeBuffer, true, 0, positions.size() * sizeof(Position), positions.data(), 0, nullptr, nullptr);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to update positions_computeBuffer" << debuglogger::endl;
		return true;
	}
	return false;
}

bool reallocateFrameComputeBuffer() {
	cl_int err = clReleaseMemObject(outputFrame_computeImage);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to release outputFrame_computeImage" << debuglogger::endl;
		return true;
	}

	return allocateFrameComputeBuffer();
}

bool setKernelWindowSizeArgs() {
	cl_int err = clSetKernelArg(computeKernel, 3, sizeof(unsigned int), &windowWidth);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set windowWidth kernel arg" << debuglogger::endl;
		return true;
	}
	err = clSetKernelArg(computeKernel, 4, sizeof(unsigned int), &windowHeight);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set windowHeight kernel arg" << debuglogger::endl;
		return true;
	}
	return false;
}

bool setKernelSizeArgs() {
	int positions_count = positions.size();
	cl_int err = clSetKernelArg(computeKernel, 1, sizeof(int), &positions_count);
	if (err != CL_SUCCESS) {
		debuglogger::out << debuglogger::error << "failed to set positions_count kernel arg" << debuglogger::endl;
		return true;
	}

	return setKernelWindowSizeArgs();
}

#define POST_THREAD_EXIT if (!PostMessage(hWnd, UWM_EXIT_FROM_THREAD, 0, 0)) { debuglogger::out << debuglogger::error << "failed to post UWM_EXIT_FROM_THREAD message to window queue" << debuglogger::endl; }
#define EXIT_FROM_THREAD POST_THREAD_EXIT goto OpenCLRelease_all;

void graphicsLoop(HWND hWnd) {
	windowWidth = newWindowWidth;
	windowHeight = newWindowHeight;
	windowResized = false;

	// All this needs to be initialized up here so that the compiler doesn't complain about the goto's below.
	size_t outputFrame_size = windowWidth * windowHeight * 4;
	char* outputFrame = new char[outputFrame_size];

	HDC finalG = GetDC(hWnd);
	HDC g = CreateCompatibleDC(finalG);
	HBITMAP bmp = CreateCompatibleBitmap(finalG, windowWidth, windowHeight);
	SelectObject(g, bmp);

	size_t computeGlobalSize[2];
	computeGlobalSize[1] = windowHeight;
	size_t computeLocalSize[2];
	computeLocalSize[1] = 1;
	size_t computeFrameOrigin[3] = { 0, 0, 0 };
	size_t computeFrameRegion[3] = { windowWidth, windowHeight, 1 };

	if (setupComputeDevice()) {
		debuglogger::out << debuglogger::error << "failed to set up compute device" << debuglogger::endl;
		POST_THREAD_EXIT;
		goto OpenCLRelease_freeLib;
	}

	positions.push_back({ 100, 100 });
	positions.push_back({ 200, 200 });

	if (allocateComputeBuffers()) {
		debuglogger::out << debuglogger::error << "failed to set up compute buffers" << debuglogger::endl;
		POST_THREAD_EXIT;
		goto OpenCLRelease_kernelAndContextVars;
	}

	if (setKernelSizeArgs()) {
		debuglogger::out << debuglogger::error << "failed to set kernel size args" << debuglogger::endl;
		EXIT_FROM_THREAD;
	}

	// Do the rest of the initialization for the sizes that rely on newly calculated data.
	computeGlobalSize[0] = windowWidth + (computeKernelWorkGroupSize - (windowWidth % computeKernelWorkGroupSize));
	computeLocalSize[0] = computeKernelWorkGroupSize;

	while (isAlive) {
		cl_int err = clEnqueueNDRangeKernel(computeCommandQueue, computeKernel, 2, nullptr, computeGlobalSize, computeLocalSize, 0, nullptr, nullptr);
		if (err != CL_SUCCESS) {
			debuglogger::out << debuglogger::error << "failed to enqueue compute kernel" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}

		err = clEnqueueReadImage(computeCommandQueue, outputFrame_computeImage, true, computeFrameOrigin, computeFrameRegion, 0, 0, outputFrame, 0, nullptr, nullptr);
		if (err != CL_SUCCESS) {
			debuglogger::out << debuglogger::error << "failed to read outputFrame_computeImage from compute device" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}
		if (!SetBitmapBits(bmp, outputFrame_size, outputFrame)) {
			debuglogger::out << debuglogger::error << "failed to set bmp bits" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}
		if (!BitBlt(finalG, 0, 0, windowWidth, windowHeight, g, 0, 0, SRCCOPY)) {
			debuglogger::out << debuglogger::error << "failed to copy g into finalG" << debuglogger::endl;
			EXIT_FROM_THREAD;
		}

		if (mouseUpdated) {
			positions[0].x = windowMouseX;
			positions[0].y = windowMouseY;
			if (updatePositionsComputeBuffer()) {
				debuglogger::out << debuglogger::error << "failed to move metaball to mouse" << debuglogger::endl;
				EXIT_FROM_THREAD;
			}
			mouseUpdated = false;
		}

		if (windowResized) {
			windowWidth = newWindowWidth;
			windowHeight = newWindowHeight;

			if (reallocateFrameComputeBuffer()) {
				debuglogger::out << debuglogger::error << "failed to reallocate outputFrame_computeImage" << debuglogger::endl;
				EXIT_FROM_THREAD;
			}

			if (setFrameKernelArg()) {
				debuglogger::out << debuglogger::error << "failed to put newly allocated outputFrame_computeImage pointer into kernel" << debuglogger::endl;
				EXIT_FROM_THREAD;
			}

			if (setKernelWindowSizeArgs()) {
				debuglogger::out << debuglogger::error << "failed to update window size args in kernel" << debuglogger::endl;
				EXIT_FROM_THREAD;
			}

			computeGlobalSize[0] = windowWidth + (computeKernelWorkGroupSize - (windowWidth % computeKernelWorkGroupSize));
			computeGlobalSize[1] = windowHeight;
			computeLocalSize[0] = computeKernelWorkGroupSize;
			computeLocalSize[1] = 1;
			computeFrameRegion[0] = windowWidth;
			computeFrameRegion[1] = windowHeight;

			DeleteDC(g);
			g = CreateCompatibleDC(finalG);										// TODO: Find a way to unselect the bmp before deleting it, instead of having to do this here.
			DeleteObject(bmp);
			bmp = CreateCompatibleBitmap(finalG, windowWidth, windowHeight);
			SelectObject(g, bmp);
			outputFrame_size = windowWidth * windowHeight * 4;
			delete[] outputFrame;
			outputFrame = new char[outputFrame_size];

			windowResized = false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));					// TODO: Make this happen with FrameManager.
	}

OpenCLRelease_all:
	if (releaseComputeMemoryObjects()) { debuglogger::out << debuglogger::error << "failed to release compute memory objects" << debuglogger::endl; }
OpenCLRelease_kernelAndContextVars:
	if (releaseComputeKernelVars()) { debuglogger::out << debuglogger::error << "failed to release compute kernel vars" << debuglogger::endl; }
	if (releaseComputeContextVars()) { debuglogger::out << debuglogger::error << "failed to release compute context vars" << debuglogger::endl; }
OpenCLRelease_freeLib:
	if (!freeOpenCLLib()) { debuglogger::out << debuglogger::error << "failed to free OpenCL library" << debuglogger::endl; }

	delete[] outputFrame;
	if (!ReleaseDC(hWnd, finalG)) { debuglogger::out << debuglogger::error << "failed to release window DC (finalG)" << debuglogger::endl; }
	if (!DeleteDC(g)) { debuglogger::out << debuglogger::error << "failed to delete memory DC (g)" << debuglogger::endl; }
	if (!DeleteObject(bmp)) { debuglogger::out << debuglogger::error << "failed to delete bmp" << debuglogger::endl; }							// This needs to be deleted after it is no longer selected by any DC.

	// TODO: If this were perfect, this thread would exit with EXIT_FAILURE as well as the main thread if something bad happened, it doesn't yet.
}