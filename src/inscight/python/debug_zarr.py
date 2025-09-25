import zarr
import sys
import numpy as np

if len(sys.argv) != 2:
    print("Usage: python debug_zarr.py <path_to_zarr>")
    exit(1)

zarr_path = sys.argv[1]

try:
    store = zarr.open_group(zarr_path, mode='r')

    print("=== DEBUG ZARR CONTENTS ===")

    # Check timestamps
    timestamps = store['timestamps']
    print(f"Timestamps shape: {timestamps.shape}")
    print(f"Non-zero timestamps count: {np.count_nonzero(timestamps[:])}")
    print(f"First 10 timestamps: {timestamps[:10]}")

    # Check commands - look for non-"UNKNOWN" entries
    commands = store['commands']
    print(f"\nCommands shape: {commands.shape}")

    # Convert first few entries back to strings
    for i in range(min(10, commands.shape[0])):
        cmd_bytes = commands[i, :]
        # Find null terminator
        null_pos = np.where(cmd_bytes == 0)[0]
        if len(null_pos) > 0:
            cmd_str = ''.join(chr(b) for b in cmd_bytes[:null_pos[0]])
        else:
            cmd_str = ''.join(chr(b) for b in cmd_bytes if b != 0)
        print(f"Command {i}: '{cmd_str}'")

        if cmd_str != "UNKNOWN":
            print(f"  -> Found non-UNKNOWN command at index {i}!")

    # Check responses
    responses = store['responses']
    print(f"\nResponses shape: {responses.shape}")

    # Convert first few entries back to strings
    for i in range(min(5, responses.shape[0])):
        resp_bytes = responses[i, :]
        null_pos = np.where(resp_bytes == 0)[0]
        if len(null_pos) > 0:
            resp_str = ''.join(chr(b) for b in resp_bytes[:null_pos[0]])
        else:
            resp_str = ''.join(chr(b) for b in resp_bytes if b != 0)
        print(f"Response {i}: '{resp_str}'")

except Exception as e:
    print(f"Error: {e}")