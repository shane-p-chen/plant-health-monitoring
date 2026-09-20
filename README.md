# Plant Health Monitoring Using Machine Learning

A machine learning project investigating whether spectral sensor data cna be used to classify plant health conditions.

The porject combines an Arduino-based spectral sensing setup, controlled data collection, data preprocessing, and multiple machine learning and deep learning models to evaluate plant health classification from spectral measurements.

## Project Objectives

The project aimed to investigate whether spectral measurements of plant leaves could be used to distinguish different plant health conditions.

The work involved:

- Building and configuring an Arduino-based spectral sensing prototype
- Collecting leaf spectral measurements under controlled lighting conditions
- Preparing and preprocessing the collected dataset
- Training and comapring machine learning and deep learning models
- Evaluating classification performance across different approaches

## System and Data Collection

The experimental setup used an Arduino-based system with an AS7265x spectral sensor to capture speectral measurements from plant leaves.

The university provided the Arduino kit and hardware components. The components required for the experiment were assembled and configured for the sensing setup, with measurements collected in a controlled black-box environment to reduce interference from external lighting.

Data was collected under three lighting conditions and across 12 spectral bands for three plant health conditions.

The Arduino program used for data collection is included in `Plant_monitor.ino`.

## Dataset

The collected data is stored in `Leaf.csv`.

The experiemtn used 25 leaf samples and produced approximately 200 spectral datasets/readings across different health and lighting conditions.

The dataset was subsequently prepared and processed in Python for machine learning analysis.

## Machine Learning Models

Four classification approaches were developed and compared:

- Decision Tree
- Support Vector Machine (SVM)
- Neural Network (Multi-Layer Perceptron / MLP)
- Convolutional Neural Network (CNN)

The complete preoprocessing, model training, and evaluation workflow is available in `Plant Classification.ipynb`.

## Results

The models were evaluated to compare the effectiveness of traditional machine learning and deep learning approaches for classifying plant health from spectral data.

The best performing model was the **Support Vector Machine (SVM)**, which achieved a classification accuracy of approximately **78.3%**.

The project demonstrate that spectral sensor measurements combined with machine learning can provide useful information for distinguishing plant health conditions, while also highlighting the limitation associated with dataset size, experimental conditions, and model generalization.

## Project Structure

/plant-health-monitoring
    Plant_monitor.ino               # Arduino spectral data collection program
    Plant Classification.ipynb      # Data processing, model training, and evaluation
    Leaf.csv                        # Collected spectral dataset
    README.md                       # Project documentation
    requirements.txt                # Python dependencies
    .gitignore                      # Git exclusions

## Technologies

### Hardware and Data Collection

- Arduino
- AS7265x spectral sensor
- Controlled lighting environment

### Data Analysis and Machine Learning

- Python
- pandas
- NumPy
- scikit-learn
- TensorFlow
- Matplotlib
- Seaborn

### Models

- Decision Tree
- Support Vector Machine
- Nerual Network (Multi-Layer Perception)
- Convolutional Nerual Network
