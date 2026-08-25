def hex_to_bits(hex_string):
    # Comprobar que la cadena solo contiene caracteres hexadecimales válidos
    valid_hex_chars = '0123456789ABCDEFabcdef'
    if not all(char in valid_hex_chars for char in hex_string):
        raise ValueError("La cadena contiene caracteres no válidos para un código hexadecimal")

    # Convertir cada carácter hexadecimal en su representación binaria de 4 bits
    bits_array = []
    for char in hex_string:
        bin_value = bin(int(char, 16))[2:]
        bin_value = bin_value.zfill(4)
        bits_array.extend([int(bit) for bit in bin_value])

    return bits_array


def binary_to_int(inicio, fin):
    binary_string = ''.join(map(str, bits_array[inicio:fin]))
    decimal_value = int(binary_string, 2)

    return decimal_value


while 1:
    hex_string = input("Introduce una cadena hexadecimal: ")
    try:
        bits_array = hex_to_bits(hex_string)
        #print(f"El arreglo de bits es: {bits_array}")
    except ValueError as e:
        print(e)

    print("Longitud del dato: ", int(len(bits_array)/4), " bytes")

    Mensaje = binary_to_int(0, 2)
    print("Mensaje", Mensaje)

    if Mensaje == 0:

        Tipo_Medidor = binary_to_int(2, 4)
        if Tipo_Medidor == 0:
            print("Monofasico Unidireccional")
        if Tipo_Medidor == 1:
            print("Monofasico Bidireccional")
        if Tipo_Medidor == 2:
            print("Trifasico")
        if Tipo_Medidor == 3:
            print("Grandes Clientes")

        Estado = binary_to_int(4, 7)
        if Estado == 0:
            print("Normal")
        if Estado == 1:
            print("Sin Energia")
        if Estado == 2:
            print("Sin Lectura")
        if Estado == 3:
            print("Sin Energia y Sin Lectura")

        Bateria = binary_to_int(7, 13)
        print("Bateria: ", Bateria)

        FP = binary_to_int(13, 20)
        print("Factor de Potencia: ", FP)

        Va = binary_to_int(20, 31)
        print("Tension A: ", Va)
        Vb = binary_to_int(31, 42)
        print("Tension B: ", Vb)
        Vc = binary_to_int(42, 53)
        print("Tension C: ", Vc)

        Ia = binary_to_int(53, 66)
        print("Corriente A: ", Ia)
        Ib = binary_to_int(66, 79)
        print("Corriente B: ", Ib)
        Ic = binary_to_int(79, 92)
        print("Corriente C: ", Ic)

        Energia_Activa_Imp = binary_to_int(92, 119)
        print("Energia Activa Importada: ", Energia_Activa_Imp)

        if Tipo_Medidor == 3:
            Energia_Activa_Exp = binary_to_int(119, 146)
            print("Energia Activa Exportada: ", Energia_Activa_Exp)
            Energia_Reactiva_Imp = binary_to_int(146, 173)
            print("Energia Reactiva Importada: ", Energia_Reactiva_Imp)
            Temperatura = binary_to_int(173, 180)
            print("Temperatura: ", Temperatura)

    if Mensaje == 1:

        Tipo_Medidor = bits_array[127]

        if Tipo_Medidor == 0: #Pequenos Clientes
            print("Monofasico o Trifasico")
            Energia_Activa_Exp = binary_to_int(2, 29)
            print("Energia Activa Exportada: ", Energia_Activa_Exp)
            Energia_Reactiva_Imp = binary_to_int(29, 56)
            print("Energia Reactiva Importada: ", Energia_Reactiva_Imp)
            Energia_Reactiva_Exp = binary_to_int(56, 83)
            print("Energia Reactiva Importada: ", Energia_Reactiva_Exp)
            Maxima_Demanda_Imp = binary_to_int(83, 103)
            print("Maxima Demanda Importada: ", Maxima_Demanda_Imp)
            Maxima_Demanda_Exp = binary_to_int(103, 123)
            print("Maxima Demanda Exportada: ", Maxima_Demanda_Exp)

        else: #Grandes Clientes
            print("Grandes Clientes")
            Energia_Reactiva_Exp = binary_to_int(2, 29)
            print("Energia Reactiva Exportada: ", Energia_Reactiva_Exp)
            Maxima_Demanda_Imp_T1 = binary_to_int(29, 49)
            print("Maxima Demanda Importada Tarifa 1: ", Maxima_Demanda_Imp_T1)
            Maxima_Demanda_Imp_T2 = binary_to_int(49, 69)
            print("Maxima Demanda Importada Tarifa 2: ", Maxima_Demanda_Imp_T2)
            Maxima_Demanda_Imp_T3 = binary_to_int(69, 89)
            print("Maxima Demanda Importada Tarifa 3: ", Maxima_Demanda_Imp_T3)

    if Mensaje == 2:

        Tipo_Medidor = bits_array[191]

        if Tipo_Medidor == 0: #Pequenos Clientes
            print("Monofasico o Trifasico")

        else: #Grandes Clientes
            print("Grandes Clientes")
            Energia_Activa_Importada_T1 = binary_to_int(2, 29)
            print("Energia Activa Importada Tarifa 1: ", Energia_Activa_Importada_T1)
            Energia_Activa_Importada_T2 = binary_to_int(29, 56)
            print("Energia Activa Importada Tarifa 2: ", Energia_Activa_Importada_T2)
            Energia_Activa_Importada_T3 = binary_to_int(56, 83)
            print("Energia Activa Importada Tarifa 3: ", Energia_Activa_Importada_T3)
            Energia_Reactiva_Importada_T1 = binary_to_int(83, 110)
            print("Energia Reactiva Importada Tarifa 1: ", Energia_Reactiva_Importada_T1)
            Energia_Activa_Importada_F1 = binary_to_int(110, 137)
            print("Energia Activa Importada Fase 1: ", Energia_Activa_Importada_F1)
            Energia_Activa_Importada_F2 = binary_to_int(137, 164)
            print("Energia Activa Importada Fase 2: ", Energia_Activa_Importada_F2)
            Energia_Activa_Importada_F3 = binary_to_int(164, 191)
            print("Energia Activa Importada Fase 3: ", Energia_Activa_Importada_F3)

    if Mensaje == 3:

        Tipo_Medidor = bits_array[127]

        if Tipo_Medidor == 0: #Pequenos Clientes
            print("Monofasico o Trifasico")

        else: #Grandes Clientes
            print("Grandes Clientes")
            Energia_Activa_Exportada_F1 = binary_to_int(2, 29)
            print("Energia Activa Exportada Fase 1: ", Energia_Activa_Exportada_F1)
            Energia_Activa_Exportada_F2 = binary_to_int(29, 56)
            print("Energia Activa Exportada Fase 2: ", Energia_Activa_Exportada_F2)
            Energia_Activa_Exportada_F3 = binary_to_int(56, 83)
            print("Energia Activa Exportada Fase 3: ", Energia_Activa_Exportada_F3)
            FP_Minimo = binary_to_int(83, 90)
            print("Factor de Potencia Minimo: ", FP_Minimo)
            FP_Promedio = binary_to_int(90, 97)
            print("Factor de Potencia Promedio: ", FP_Promedio)
            Frecuencia_Minima = binary_to_int(97, 110)
            print("Frecuencia Minima: ", Frecuencia_Minima)
            Frecuencia_Maxima = binary_to_int(110, 123)
            print("Frecuencia Maxima: ", Frecuencia_Maxima)
