
import cv2
import numpy as np
import argparse
import os

def extract_and_save_frames(video_path, frames_list, output_dir):
    """
    Extracts specified frames from a video file and saves them as PNG images and binary arrays.

    Args:
    - video_path (str): Path to the video file.
    - frames_list (list of int): List of frame numbers to extract.
    - output_dir (str): Directory to save the extracted frames and binary arrays.
    """
    # Ensure the output directory exists
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Open the video file
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print(f"Error opening video file {video_path}")
        return

    frame_idx = 0
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        if frame_idx in frames_list:
            # Save frame as PNG
            frame_path = os.path.join(output_dir, f"frame_{frame_idx}.png")
            cv2.imwrite(frame_path, frame)
            print(f"Saved {frame_path}")

            # Convert frame to uint8 binary array and save
            frame_bin_path = os.path.join(output_dir, f"frame_{frame_idx}.bin")
            with open(frame_bin_path, 'wb') as f_bin:
                frame_gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                f_bin.write(frame_gray.astype(np.uint8).tobytes())
            print(f"Saved {frame_bin_path}")

        frame_idx += 1

    cap.release()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Extract frames from a video file.")
    parser.add_argument("video_path", type=str, help="Path to the video file.")
    parser.add_argument("frames_list", type=int, nargs='+', help="List of frame numbers to extract.")
    parser.add_argument("output_dir", type=str, help="Directory to save the extracted frames and binary arrays.")
    args = parser.parse_args()

    extract_and_save_frames(args.video_path, args.frames_list, args.output_dir)