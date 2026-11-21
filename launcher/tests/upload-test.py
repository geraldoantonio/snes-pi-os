#!/usr/bin/env python3
"""Testa o snespi-upload: envios normais e entradas malformadas.

uso: upload-test.py CAMINHO_DO_BINARIO

O binário deve vir compilado com ASan/UBSan (make test-upload): qualquer
mensagem do sanitizador na saída de erro reprova o teste.
"""
import os
import random
import signal
import socket
import subprocess
import sys
import tempfile
import time

PIN = "4821"
B = "----snespiTeste7MA4YWxkTrZu0gW"
falhas = []


def confere(cond, msg):
    if not cond:
        falhas.append(msg)
        print("FALHOU:", msg)


class Servidor:
    def __init__(self, binario, pasta, max_mb=1):
        s = socket.socket()
        s.bind(("127.0.0.1", 0))
        self.porta = s.getsockname()[1]
        s.close()
        self.pasta = pasta
        self.err = tempfile.TemporaryFile()
        self.proc = subprocess.Popen(
            [binario, "--dir", pasta, "--pin", PIN, "--port", str(self.porta), "--max-mb", str(max_mb)],
            stdout=subprocess.PIPE, stderr=self.err)
        linha = self.proc.stdout.readline().decode()
        assert linha.startswith("pronto\t"), linha
        os.set_blocking(self.proc.stdout.fileno(), False)
        self.eventos = []

    def le_eventos(self, espera=0.3):
        time.sleep(espera)
        try:
            dados = self.proc.stdout.read() or b""
        except BlockingIOError:
            dados = b""
        novos = [l for l in dados.decode("utf-8", "replace").split("\n") if l]
        self.eventos += novos
        return novos

    def bruto(self, dados, pedacos=None, fecha_escrita=False, espera_resposta=True):
        """Manda bytes crus e devolve a resposta inteira (b'' se a conexão caiu)."""
        c = socket.create_connection(("127.0.0.1", self.porta), timeout=10)
        try:
            if pedacos:
                for i in range(0, len(dados), pedacos):
                    c.sendall(dados[i:i + pedacos])
            else:
                c.sendall(dados)
            if fecha_escrita:
                c.shutdown(socket.SHUT_WR)
            if not espera_resposta:
                return b""
            resp = b""
            while True:
                r = c.recv(65536)
                if not r:
                    break
                resp += r
            return resp
        except OSError:
            return b""
        finally:
            c.close()

    def get(self, caminho):
        return self.bruto(("GET %s HTTP/1.1\r\nHost: x\r\n\r\n" % caminho).encode())

    def envia(self, arquivos, pedacos=None, caminho=None):
        corpo = b""
        for nome, dados in arquivos:
            corpo += ("--%s\r\nContent-Disposition: form-data; name=\"arquivo\"; filename=\"%s\"\r\n"
                      "Content-Type: application/octet-stream\r\n\r\n" % (B, nome)).encode() + dados + b"\r\n"
        corpo += ("--%s--\r\n" % B).encode()
        return self.bruto(self.cabecalho(len(corpo), caminho) + corpo, pedacos)

    def cabecalho(self, tamanho, caminho=None):
        return ("POST %s HTTP/1.1\r\nHost: x\r\nContent-Type: multipart/form-data; boundary=%s\r\n"
                "Content-Length: %d\r\n\r\n" % (caminho or "/enviar?pin=%s" % PIN, B, tamanho)).encode()

    def temporarios(self):
        d = os.path.join(self.pasta, ".snespi")
        return [f for f in os.listdir(d)] if os.path.isdir(d) else []

    def encerra(self):
        self.proc.send_signal(signal.SIGTERM)
        rc = self.proc.wait(timeout=10)
        self.err.seek(0)
        erro = self.err.read().decode("utf-8", "replace")
        return rc, erro


def le(pasta, nome):
    try:
        with open(os.path.join(pasta, nome), "rb") as f:
            return f.read()
    except OSError:
        return None


def corpo_de(resp):
    return resp.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in resp else b""


def main():
    binario = sys.argv[1]
    random.seed(1)
    with tempfile.TemporaryDirectory() as pasta:
        s = Servidor(binario, pasta)

        # página na raiz (sem PIN) e conferência do PIN
        r = s.get("/")
        confere(r.startswith(b"HTTP/1.1 200") and b"Enviar jogos" in r and b"Digite o PIN" in r, "GET / devolve a página")
        confere(s.get("/?pin=" + PIN).startswith(b"HTTP/1.1 200"), "GET /?pin= (QR code) devolve a página")
        confere(PIN.encode() not in r, "a página não traz o PIN")
        confere(s.get("/pin?pin=" + PIN).startswith(b"HTTP/1.1 200"), "GET /pin com o PIN certo devolve 200")
        confere(s.get("/pin?a=1&pin=" + PIN).startswith(b"HTTP/1.1 200"), "PIN em qualquer posição da query")
        for ruim in ("/pin", "/pin?pin=", "/pin?pin=0000", "/pin?pin=" + PIN + "5", "/pin?pin=" + PIN[:3],
                     "/pin?xpin=" + PIN):
            confere(s.get(ruim).startswith(b"HTTP/1.1 403"), "GET %s devolve 403" % ruim)
        for ruim in ("/" + PIN, "/favicon.ico", "/enviar", "/outra?pin=" + PIN):
            confere(s.get(ruim).startswith(b"HTTP/1.1 404"), "GET %s devolve 404" % ruim)
        for caminho in ("/enviar?pin=0000", "/enviar"):
            confere(s.envia([("a.sfc", b"x" * 10)], caminho=caminho).startswith(b"HTTP/1.1 403"),
                    "POST %s devolve 403" % caminho)
        confere(le(pasta, "a.sfc") is None, "POST sem o PIN certo não grava")

        # ROM com dados que parecem delimitador, em pedaços pequenos
        rom = bytes(random.getrandbits(8) for _ in range(200000))
        rom = rom[:5000] + b"\r\n--" + B.encode()[:-1] + b"\r\n--\r\n\r" + rom[5000:]
        r = s.envia([("Jogo Um (BR).sfc", rom)], pedacos=1777)
        confere(corpo_de(r) == b"ok\tJogo Um (BR).sfc\n", "envio de ROM responde ok: %r" % r[:200])
        confere(le(pasta, "Jogo Um (BR).sfc") == rom, "ROM gravada igual à enviada")
        ev = s.le_eventos()
        confere("inicio\tJogo Um (BR).sfc" in ev and "ok\t%d\trom\tJogo Um (BR).sfc" % len(rom) in ev,
                "eventos inicio e ok: %r" % ev)

        # capa, nome com acento, vários arquivos numa requisição, sobrescrita
        r = s.envia([("Jogo Um (BR).PNG", b"\x89PNG capa"), ("Ação.smc", b"abc"), ("Jogo Um (BR).sfc", b"novo")])
        confere(corpo_de(r).count(b"ok\t") == 3, "três arquivos numa requisição: %r" % corpo_de(r))
        confere(le(pasta, "capas/Jogo Um (BR).PNG") == b"\x89PNG capa", "capa vai para capas/")
        confere(le(pasta, "Ação.smc") == b"abc", "nome com acento")
        confere(le(pasta, "Jogo Um (BR).sfc") == b"novo", "mesmo nome sobrescreve")

        # nomes perigosos
        r = s.envia([("../../fora.sfc", b"1"), ("..\\..\\fora2.smc", b"2"), (".oculto.fig", b"3"),
                     ("a:b*c?.swc", b"4"), ("/etc/passwd", b"5"), ("virus.exe", b"6"), ("..", b"7"),
                     (".sfc", b"8"), ("x" * 400 + ".sfc", b"9")])
        linhas = corpo_de(r).decode().split("\n")
        confere(le(pasta, "fora.sfc") == b"1" and le(pasta, "fora2.smc") == b"2", "caminho vira só o nome")
        confere(not os.path.exists(os.path.join(pasta, "..", "fora.sfc")), "nada gravado fora da pasta")
        confere(le(pasta, "oculto.fig") == b"3", "ponto inicial removido")
        confere(le(pasta, "a_b_c_.swc") == b"4", "caracteres inválidos trocados")
        confere("erro\ttipo\tpasswd" in linhas and "erro\ttipo\tvirus.exe" in linhas, "tipos recusados: %r" % linhas)
        confere("erro\tnome\t?" in linhas and "erro\ttipo\tsfc" in linhas, "nomes vazios recusados: %r" % linhas)
        longo = [f for f in os.listdir(pasta) if f.startswith("xxxx")]
        confere(len(longo) == 1 and len(longo[0]) <= 200 and longo[0].endswith(".sfc"), "nome longo cortado")

        # grande demais (limite de 1 MB no teste)
        r = s.envia([("grande.sfc", b"g" * (1024 * 1024 + 5000)), ("depois.sfc", b"ok")])
        confere(b"erro\tgrande\tgrande.sfc" in r and b"ok\tdepois.sfc" in r, "arquivo grande recusado: %r" % r[-80:])
        confere(le(pasta, "grande.sfc") is None, "arquivo grande não fica no disco")

        # corpo cortado: Content-Length maior que o enviado
        parte = ("--%s\r\nContent-Disposition: form-data; name=\"arquivo\"; filename=\"corte.sfc\"\r\n\r\n" % B).encode()
        s.bruto(s.cabecalho(50000) + parte + b"z" * 1000, fecha_escrita=True)
        confere(le(pasta, "corte.sfc") is None, "envio cortado não deixa arquivo")
        # sem o delimitador final
        s.bruto(s.cabecalho(len(parte) + 1000) + parte + b"z" * 1000)
        confere(le(pasta, "corte.sfc") is None, "envio sem delimitador final não deixa arquivo")
        # arquivo vazio
        s.envia([("vazio.sfc", b"")])
        confere(le(pasta, "vazio.sfc") is None, "arquivo vazio não é gravado")

        # entradas malformadas: o servidor não pode cair nem gravar nada
        antes = sorted(os.listdir(pasta))
        lixo = [
            b"\x00\xff\xfe" * 500,
            b"GET " + b"/" * 20000 + b" HTTP/1.1\r\n\r\n",
            b"POST /enviar?pin=%s HTTP/1.1\r\n\r\n" % PIN.encode(),
            b"POST /enviar?pin=%s HTTP/1.1\r\nContent-Length: -5\r\nContent-Type: multipart/form-data; boundary=x\r\n\r\n" % PIN.encode(),
            b"POST /enviar?pin=%s HTTP/1.1\r\nContent-Length: 99999999999999999999\r\nContent-Type: multipart/form-data; boundary=x\r\n\r\nabc" % PIN.encode(),
            b"POST /enviar?pin=%s HTTP/1.1\r\nContent-Length: 10\r\nContent-Type: multipart/form-data; boundary=" % PIN.encode() + b"b" * 4000 + b"\r\n\r\n0123456789",
            b"POST /enviar?pin=%s HTTP/1.1\r\nContent-Length: 10\r\nContent-Type: text/plain\r\n\r\n0123456789" % PIN.encode(),
            s.cabecalho(40) + b"--" + B.encode() + b"XX",
            s.cabecalho(6000) + b"--" + B.encode() + b"\r\n" + b"H" * 5900,
            s.cabecalho(200) + ("--%s\r\nContent-Disposition: form-data; name=\"a\"; filename=\"\r\n\r\n" % B).encode(),
            s.cabecalho(300) + ("--%s\r\nContent-Disposition: form-data; filename=\r\n\r\nabc\r\n--%s--\r\n" % (B, B)).encode(),
            b"\r\n\r\n",
            b"GET",
        ]
        for i, dados in enumerate(lixo):
            s.bruto(dados, fecha_escrita=True)
        for _ in range(60):
            n = random.randrange(1, 3000)
            base = s.cabecalho(n) + ("--%s\r\nContent-Disposition: form-data; name=\"a\"; filename=\"f.sfc\"\r\n\r\n" % B).encode()
            ruido = bytearray(base + bytes(random.getrandbits(8) for _ in range(n)))
            for _ in range(random.randrange(0, 6)):
                ruido[random.randrange(len(ruido))] = random.getrandbits(8)
            s.bruto(bytes(ruido), fecha_escrita=True)
        depois = sorted(f for f in os.listdir(pasta) if f != "f.sfc")
        confere(antes == depois, "entradas malformadas não criam arquivos: %r" % (set(depois) - set(antes)))
        confere(s.get("/pin?pin=" + PIN).startswith(b"HTTP/1.1 200"), "servidor segue atendendo depois do lixo")

        # conexões paradas (reserva do navegador) não travam o servidor
        paradas = [socket.create_connection(("127.0.0.1", s.porta)) for _ in range(3)]
        confere(s.get("/").startswith(b"HTTP/1.1 200"), "atende com 3 conexões paradas")
        for c in paradas:
            c.close()

        confere(s.temporarios() == [], "nenhum temporário sobrou: %r" % s.temporarios())
        rc, erro = s.encerra()
        confere(rc == 0, "SIGTERM encerra com código 0 (veio %r)" % rc)
        confere("Sanitizer" not in erro and "runtime error" not in erro, "sanitizador:\n" + erro[-3000:])

        # PIN errado vezes demais: bloqueia até reabrir o modo rede
        s = Servidor(binario, pasta)
        for i in range(40):
            s.get("/")  # abrir a página não conta como tentativa
        confere(s.get("/pin?pin=" + PIN).startswith(b"HTTP/1.1 200"), "abrir a página muitas vezes não bloqueia")
        for i in range(30):
            s.get("/pin?pin=%04d" % (i + 5000))
        ev = s.le_eventos(0.5)
        confere("bloqueado" in ev, "30 PINs errados bloqueiam: %r" % ev)
        confere(not s.get("/pin?pin=" + PIN).startswith(b"HTTP/1.1 200"), "bloqueado não atende nem o PIN certo")
        rc, erro = s.encerra()
        confere(rc == 0 and "Sanitizer" not in erro, "encerra depois do bloqueio")

    if falhas:
        print("upload-test: %d falha(s)" % len(falhas))
        return 1
    print("upload-test: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
