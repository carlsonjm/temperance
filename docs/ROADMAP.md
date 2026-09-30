# Roadmap

What is planned for Temperance.

- **System events.** Sources beyond notifications, each added only when it is
  authoritative, deduplicated and dismissable, and reported as it changes rather
  than polled.
- **Progress.** Live progress and its actions stay with Ambient in Tettegouche,
  and neither side keeps the same finished item indefinitely
  (`AMBIENT-BOUNDARY.md`).
- **Confirming Log Out.** Open decision: whether Log Out asks first, as Restart
  and Shut Down do.
- **Choosing the notification presenter.** An option for Plasma's own presenter,
  once the notification lifecycle across processes is proven
  (`ARCHITECTURE.md` § One store, one presenter).
