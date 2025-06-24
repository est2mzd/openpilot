import cupy
print("CuPy version:", cupy.__version__)

print("Available GPU:", cupy.cuda.runtime.getDeviceCount())

gpu_id = cupy.cuda.runtime.getDevice()
gpu_name = cupy.cuda.runtime.getDeviceProperties(gpu_id)["name"]
print("Using GPU:", gpu_name)

free_mem, total_mem = cupy.cuda.runtime.memGetInfo()
print(f"Free memory: {free_mem / 1024**2:.1f} MB / Total: {total_mem / 1024**2:.1f} MB")

x = cupy.random.randn(1000, 1000).astype(cupy.float32)
y = cupy.dot(x, x)
print("Dot product success, shape:", y.shape)
