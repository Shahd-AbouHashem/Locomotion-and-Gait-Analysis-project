import tkinter as tk
from tkinter import ttk
import serial
import threading

# Serial port settings
SERIAL_PORT = 'COM8'  # Adjust as needed
BAUD_RATE = 115200

class GaitAnalysisGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("🦶 Locomotion and Gait Analysis")
        self.root.configure(bg="#eaf2f8")

        header_font = ("Segoe UI", 20, "bold")
        label_font = ("Segoe UI", 12)
        data_font = ("Consolas", 12, "bold")

        tk.Label(root, text="Gait Analysis Dashboard", font=header_font, bg="#eaf2f8", fg="#2c3e50").pack(pady=(20, 10))

        self.main_frame = ttk.LabelFrame(root, text="Realtime Parameters", padding=20)
        self.main_frame.pack(pady=10, padx=10, fill="both", expand=True)

        self.labels = {}
        fields = [
            "Heel Pressure", "MTH Pressure", "Toe Pressure",
            "Shank Angle", "Ankle Angle",
            "Step Time (ms)", "Cadence (steps/min)",
            "Center of Pressure (CoP)", "Symmetry Index"
        ]
        for field in fields:
            frame = tk.Frame(self.main_frame, bg="#ffffff")
            frame.pack(anchor="w", pady=4, fill="x")
            label = tk.Label(frame, text=f"{field}:", font=label_font, bg="#ffffff", width=22, anchor="w")
            label.pack(side="left")
            value_label = tk.Label(frame, text="---", font=data_font, bg="#ffffff", fg="#2980b9", anchor="w")
            value_label.pack(side="left")
            self.labels[field] = value_label

        # Gait Phase
        frame = tk.Frame(self.main_frame, bg="#ffffff")
        frame.pack(anchor="w", pady=4, fill="x")
        label = tk.Label(frame, text="Gait Phase:", font=label_font, bg="#ffffff", width=22, anchor="w")
        label.pack(side="left")
        self.gait_phase_label = tk.Label(frame, text="---", font=data_font, bg="#ffffff", fg="#34495e", anchor="w")
        self.gait_phase_label.pack(side="left")

        # FSR indicators
        self.indicator_frame = ttk.LabelFrame(root, text="Foot Pressure Indicators", padding=10)
        self.indicator_frame.pack(pady=(5, 10))
        self.fsr_boxes = {}
        for region in ["Heel", "MTH", "Toe"]:
            frame = tk.Frame(self.indicator_frame, padx=20, pady=5, bg="#eaf2f8")
            frame.pack(side="left", padx=15)
            box = tk.Canvas(frame, width=60, height=60, bg="#bdc3c7", bd=1, relief="solid")
            box.pack()
            tk.Label(frame, text=region, bg="#eaf2f8", font=label_font).pack()
            self.fsr_boxes[region] = box

        # Stability Status Placeholder
        self.stability_frame = ttk.LabelFrame(root, text="Stability Status", padding=10)
        self.stability_frame.pack(pady=(5, 20))
        self.stability_label = tk.Label(self.stability_frame, text="---", font=("Segoe UI", 14, "bold"), fg="#8e44ad")
        self.stability_label.pack()

        self.running = True
        self.serial_thread = threading.Thread(target=self.read_serial)
        self.serial_thread.daemon = True
        self.serial_thread.start()

    def update_ui(self, data):
        try:
            heel, rmet, lmet, toe, mth, ankle_angle, shank_angle, cop_x, cop_y, phase, step_time, cadence, symmetry = data

            self.labels["Heel Pressure"].config(text=heel)
            self.labels["MTH Pressure"].config(text=f"{rmet}/{lmet} ({float(mth):.1f})")
            self.labels["Toe Pressure"].config(text=toe)
            self.labels["Shank Angle"].config(text=f"{float(shank_angle):.2f}°")
            self.labels["Ankle Angle"].config(text=f"{ankle_angle}°")
            self.labels["Step Time (ms)"].config(text=step_time)
            self.labels["Cadence (steps/min)"].config(text=f"{float(cadence):.1f}")
            self.labels["Center of Pressure (CoP)"].config(text=f"X: {float(cop_x):.2f}, Y: {float(cop_y):.2f}")
            self.labels["Symmetry Index"].config(text=f"{float(symmetry):.2f}")
            self.gait_phase_label.config(text=phase)

            # Optional placeholder for stability (not in Arduino data now)
            self.stability_label.config(text="---")

            # FSR box colors
            self.fsr_boxes["Heel"].config(bg="#27ae60" if int(heel) > 100 else "#bdc3c7")
            self.fsr_boxes["MTH"].config(bg="#27ae60" if int(rmet) > 100 or int(lmet) > 100 else "#bdc3c7")
            self.fsr_boxes["Toe"].config(bg="#27ae60" if int(toe) > 100 else "#bdc3c7")

        except Exception as e:
            print(f"UI update error: {e}")

    def read_serial(self):
        try:
            with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
                while self.running:
                    try:
                        line = ser.readline().decode().strip()
                        if not line or ',' not in line:
                            continue
                        parts = line.split(',')
                        if len(parts) != 13:
                            continue

                        self.root.after(0, self.update_ui, parts)

                    except Exception as err:
                        print("Serial read error:", err)
        except serial.SerialException:
            print(f"Could not open serial port {SERIAL_PORT}. Is the device connected?")

    def stop(self):
        self.running = False

if __name__ == "__main__":
    root = tk.Tk()
    app = GaitAnalysisGUI(root)
    try:
        root.mainloop()
    finally:
        app.stop()
