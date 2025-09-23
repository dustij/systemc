import zarr

# Read zarr data from data.zr
store = zarr.open('data.zr', mode='r')
print(f"Zarr store keys: {list(store.keys())}")

# Print basic info about each array
for key in store.keys():
    array = store[key]
    print(f"{key}: shape={array.shape}, dtype={array.dtype}")