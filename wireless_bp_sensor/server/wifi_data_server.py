import matplotlib
matplotlib.use("Agg")

from flask import Flask, request, jsonify, render_template_string, send_file
import matplotlib.pyplot as plt
import os

app = Flask(__name__)

latest_freq_mhz = None  # to display on the page

@app.route("/data", methods=["POST"])
def receive_data():
    global latest_freq_mhz

    data = request.get_json()
    freq_khz = data["freq"]
    adc = data["adc"]

    freq_mhz = [f / 1000.0 for f in freq_khz]

    # Find the frequency corresponding to the max ADC value
    if adc:
        max_index = adc.index(max(adc))
        latest_freq_mhz = freq_mhz[max_index]
    else:
        latest_freq_mhz = None

    # --- Plot ---
    plt.figure()
    plt.scatter(freq_mhz, adc, color="blue", s=20)
    plt.title("ADC Samples vs Frequency")
    plt.xlabel("Frequency (MHz)")
    plt.ylabel("ADC Value")
    plt.grid(True)
    plt.savefig("latest_plot.png", dpi=150, bbox_inches="tight")
    plt.close()

    return jsonify({"status": "ok"})


@app.route("/")
def index():
    if latest_freq_mhz is not None:
        info = f"Peak Frequency: {latest_freq_mhz:.3f} MHz"
    else:
        info = "No data received yet."

    html = f"""
    <html>
        <head><title>ADC Sweep</title></head>
        <body style="font-family: sans-serif; text-align: center; margin-top: 50px;">
            <h2>ADC Samples vs Frequency</h2>
            <p>{info}</p>
            <img src="/plot.png" alt="ADC Plot" width="600">
        </body>
    </html>
    """
    return render_template_string(html)


@app.route("/plot.png")
def plot_png():
    if os.path.exists("latest_plot.png"):
        return send_file("latest_plot.png", mimetype="image/png")
    else:
        return "No plot yet", 404


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
